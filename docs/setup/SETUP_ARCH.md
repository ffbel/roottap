# Setup system
```
./scripts/setup_arch.sh
```
# Install espidf framework
```
./scripts/espidf_install.sh
```
# Install Android Studio
```
./scripts/install_android_studio_system.sh
./scripts/arch_enable_kvm.sh

# run this command and choose `android sdk to be installed to dir set in script `scripts/install_android_studio_system`
android-studio
```

# Check everything works

```
sudo reboot
```

After reboot check this commands work
```
adb devices
virt-host-validate
emulator -accel-check
```