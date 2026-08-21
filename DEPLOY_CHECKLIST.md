# DEPLOY CHECKLIST  -  Alterações Irreversíveis em Produção


---

> Antes de executar qualquer operação marcada como irreversível percorra **todos** os itens abaixo.

---

## FASE 0  -  Classificação da Mudança
- Migração ou alteração de schema de banco de dados
- Implementação ou modificação de autenticação (2FA, OAuth, SSO)
- Rotação de chaves criptográficas
- Operação DROP, TRUNCATE ou ALTER TABLE sem rollback trivial

---

## FASE 1  -  Pré-Condições de Ambiente
- Backup completo (dados + schema) realizado nas últimas 2 horas
- Hash SHA-256 do backup registrado
- Restauração do backup testada em ambiente isolado

---

## FASE 2  -  Shadow Deployment (Validação de Tráfego em Sombra)
- Mirror de tráfego ativo: requisições reais duplicadas para o ambiente Green/Shadow; respostas ao usuário só do Blue (produção atual)
- Mínimo 30 min de tráfego espelhado; divergência de taxa de erro Blue x Green < 0,1%; latência P99 do Green não excede o Blue em mais de 15%
- Sem Load Balancer configurável (ex.: hospedagem compartilhada): substituir por dupla validação manual em staging, documentada no `CLAUDE.md` do projeto, e avançar para a Fase 3

---

## FASE 3  -  Database Migration (Dual Writes)
- A aplicação está configurada para escrever simultaneamente no banco legado e no novo schema
- Backfill de dados históricos concluído
- Validação de consistência lógica 100% (amostra mínima de 1.000 registros)

---

## FASE 4  -  Validação de Segurança
- Bibliotecas criptográficas de fontes oficiais
- Cabeçalhos de Segurança HTTP (HSTS, CSP, X-Frame)
- Fluxo de 2FA validado (Passou)

---

## FASE 5  -  Blue-Green Cutover
- Rollback plan documentado e testado
- Health check retornando 200 OK por 5 minutos consecutivos
- Taxa de erro pós-cutover < 0,5%
- **Sub-fase 5.3 (passo irreversível):** mínimo 48h de operação estável no novo ambiente + aprovação nominal registrada (Responsável + Data-Hora + Assinatura) + banco/estado legado desativado mas mantido offline por 30 dias adicionais antes do descarte definitivo

---

## FASE 6  -  Pós-Deploy e Documentação
- Incident report: o que mudou, quando, quem aprovou, métricas de validação
- Runbook de rollback atualizado com lições aprendidas
- Alertas de monitoramento ajustados para a nova arquitetura
- Time notificado da conclusão bem-sucedida do deploy
