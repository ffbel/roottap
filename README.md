# roottap
Physical approval for privileged actions (sudo / SSH) via phone.

roottap/
├── shared/        # protocol + crypto (future server-safe)
├── mobile/        # iOS / Android (native)
├── firmware/      # ESP32 (ESP-IDF)
├── host/          # Linux PAM + tooling (sudo target)
├── infra/         # CI, signing metadata
└── docs/          # architecture, threat model
