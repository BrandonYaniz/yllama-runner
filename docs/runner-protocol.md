# Runner protocol

Protocol 1 is the compatibility default: input is `uint32_le length` plus prompt
bytes; output is `0x01 uint32_le length bytes`, terminal `0x02`, or legacy error
`0x03 uint16_le length bytes`. Its sampling settings remain startup flags.

New integrations must select protocol 2 explicitly. The complete normative
contract and yllmd migration procedure are in
[yllmd-protocol-v2-migration.md](yllmd-protocol-v2-migration.md).
