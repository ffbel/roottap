#!/usr/bin/env bash
set -euo pipefail

sudo pacman -S qemu-full libvirt virt-manager
sudo systemctl enable --now libvirtd
sudo usermod -aG libvirt,kvm $USER
# # logout/login
# virt-host-validate
# emulator -accel-check
