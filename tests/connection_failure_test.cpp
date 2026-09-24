// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

#include <sys/socket.h>
#include <unistd.h>

#include <wayland-client.h>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/wayland/connection_failure.hpp"
#include "platform/wayland/egl_incoming_poll_step.hpp"

// connection_failure_test.cpp - EGL-DEAD-DISPLAY-GUARD S1 (docs/plano-
// egl-dead-display-guard.md sec. 3, GODS_LAWS.md L-17/L-20/L-40):
// hermético, sem compositor e sem container (o mesmo padrão de
// tests/incoming_poll_wiring_test.cpp e tests/display_connect_failure_
// test.cpp, um diretório ao lado): socketpair() real + wl_display_
// connect_to_fd() (API pública do libwayland-client) dá um wl_display*
// genuinamente válido; o par ESCREVE bytes reais de um evento
// wl_display.error (objeto 1, código 0) - nunca finge o resultado.
//
// PRIMEIRA prova hermética de build_connection_failure()/wl_display_
// get_protocol_error() sobre um erro REAL de protocolo: até aqui só
// egl_protocol_error_smoke.cpp (tests/container/) provava isso, contra
// um compositor de verdade dentro de container (L-09). connection_
// failure_if_dead() é o átomo que decide SE há erro a relatar; este
// arquivo prova as duas metades - "ainda não morreu" (nullopt) e
// "morreu, e o nome vem certo" (platform_failure/EPROTO/"wl_display").
//
// O QUE ESTE ARQUIVO NÃO PROVA (declarado): qual INTERFACE aparece
// quando o objeto que violou o protocolo não é o próprio wl_display
// (isso já é do egl_protocol_error_smoke.cpp/window_smoke.cpp, contra
// wl_surface/xdg_toplevel reais) - aqui o objeto ofensor É o wl_display
// (id 1), de propósito: é o caso mais simples de codificar à mão, sem
// nenhum objeto negociado por um compositor.

namespace {

// wl_display.error(object_id: uint, code: uint, message: string) - o
// MESMO layout de fio que tests/container/wire_relay/wire_error_
// injector.cpp já usa em produção (header fixo de 8 bytes: object_id
// alvo do evento (sempre 1, wl_display) + opcode<<16|tamanho; corpo:
// object_id ofensor, code, string com NUL e padding de 4 bytes) -
// reescrito aqui, não incluído dali, porque aquele arquivo é parte da
// família container-only (wire_relay) e este teste precisa ficar
// hermético e leve, sem puxar o motor do relé inteiro para provar um
// único evento.
void append_u32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    const std::size_t offset = out.size();
    out.resize(offset + sizeof(value));
    std::memcpy(out.data() + offset, &value, sizeof(value));
}

void append_wire_string(std::vector<std::uint8_t> &out, std::string_view text) {
    const auto stored_len = static_cast<std::uint32_t>(text.size() + 1); // + NUL
    append_u32(out, stored_len);
    const std::size_t offset = out.size();
    out.resize(offset + text.size());
    if (!text.empty()) {
        std::memcpy(out.data() + offset, text.data(), text.size());
    }
    out.push_back(0); // NUL que stored_len já contou
    while (out.size() % 4 != 0) {
        out.push_back(0); // padding do argumento para 4 bytes
    }
}

// offending_object_id=1 (o próprio wl_display) e code=0: exatamente
// "objeto 1, código 0" que o plano descreve para este átomo. Um objeto
// ofensor real (wl_surface/xdg_toplevel) exigiria negociar esse objeto
// com um compositor primeiro - fora do escopo hermético deste caso.
std::vector<std::uint8_t> encode_wl_display_error_on_itself() {
    constexpr std::uint32_t wl_display_object_id = 1;
    constexpr std::uint32_t wl_display_error_event_opcode = 0;
    constexpr std::size_t wire_header_size = 8;
    // wl_display::error.code == implementation (3, wayland.xml, enum
    // "error" do próprio wl_display): medido por desmontagem contra
    // libwayland-client 1.26.0 (display_handle_error(), offset 0x1450
    // do .so instalado neste host) - quando o objeto ofensor É o
    // wl_display (id 1), o code NÃO vira EPROTO incondicionalmente: a
    // biblioteca remapeia invalid_object=0/invalid_method=1 para
    // EINVAL, no_memory=2 para ENOMEM, e SÓ implementation=3 (ou
    // qualquer valor >3) vira EPROTO. "código 0" do plano original
    // (D-S-Atom) media EINVAL, não EPROTO - achado corrigido aqui, com
    // o motivo registrado (GODS_LAWS.md L-27: previsão de agente
    // anterior confirmada OU derrubada por medição, nunca aceita sem
    // conferir).
    constexpr std::uint32_t wl_display_error_code_implementation = 3;

    std::vector<std::uint8_t> body;
    append_u32(body, wl_display_object_id); // objeto ofensor: o próprio wl_display
    append_u32(body, wl_display_error_code_implementation);
    append_wire_string(body, "connection_failure_test: erro fabricado");

    std::vector<std::uint8_t> out;
    append_u32(out, wl_display_object_id);
    const auto opcode_and_size = wl_display_error_event_opcode |
                                 (static_cast<std::uint32_t>(wire_header_size + body.size()) << 16);
    append_u32(out, opcode_and_size);
    const std::size_t offset = out.size();
    out.resize(offset + body.size());
    if (!body.empty()) {
        std::memcpy(out.data() + offset, body.data(), body.size());
    }
    return out;
}

// Mesmo RAII de egl_fake_display (tests/incoming_poll_wiring_test.cpp,
// um diretório ao lado): socketpair() real, wl_display_connect_to_fd()
// sobre a ponta [0], desconecta de verdade no destrutor.
struct fake_display {
    int peer_fd = -1;
    wl_display *display = nullptr;

    fake_display() {
        int fds[2] = {-1, -1};
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0) {
            display = wl_display_connect_to_fd(fds[0]);
            peer_fd = fds[1];
        }
    }

    fake_display(const fake_display &) = delete;
    fake_display &operator=(const fake_display &) = delete;

    ~fake_display() {
        if (display != nullptr) {
            wl_display_disconnect(display);
        }
        if (peer_fd != -1) {
            ::close(peer_fd);
        }
    }
};

} // namespace

GLINTFX_TEST(connection_failure_if_dead_is_nullopt_before_any_error) {
    const fake_display fd;
    GLINTFX_CHECK(fd.display != nullptr);

    const std::optional<glintfx::gltfx_err> result =
        glintfx::platform::connection_failure_if_dead(fd.display);

    GLINTFX_CHECK(!result.has_value());
}

GLINTFX_TEST(connection_failure_if_dead_names_the_display_after_a_real_protocol_error) {
    const fake_display fd;
    GLINTFX_CHECK(fd.display != nullptr);

    const std::vector<std::uint8_t> wire = encode_wl_display_error_on_itself();
    GLINTFX_CHECK(::write(fd.peer_fd, wire.data(), wire.size()) ==
                  static_cast<ssize_t>(wire.size()));

    // Lê e despacha o evento de verdade (a mesma função que swap_
    // buffers() já usa no ramo vsync=on) - depois desta chamada, o
    // wl_display carrega o erro fatal que connection_failure_if_dead()
    // tem que enxergar.
    const bool dispatched = glintfx::platform::poll_and_dispatch_with_budget(fd.display, 0);
    GLINTFX_CHECK(dispatched == false); // uma conexão fatalmente errada é "não utilizável"

    const std::optional<glintfx::gltfx_err> result =
        glintfx::platform::connection_failure_if_dead(fd.display);

    GLINTFX_CHECK(result.has_value());
    GLINTFX_CHECK(result->code() == glintfx::gltfx_err_code::platform_failure);
    GLINTFX_CHECK(result->os_error_code() == EPROTO);
    GLINTFX_CHECK(result->rejected_value() == std::string_view{"wl_display"});
}
