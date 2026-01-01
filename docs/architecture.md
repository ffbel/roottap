```
USB HID (C)
  ↓
CTAPHID framing (C)
  ↓
CTAP core (Rust)
  ↓
needs_user_presence()
  ↓
callback into C
  ↓
BLE approval gate
  ↓
result back to Rust
```