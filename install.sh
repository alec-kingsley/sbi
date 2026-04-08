#!/bin/bash

# shell coloring
CYAN='\e[0;36m'
YELLOW='\e[0;33m'
RED='\e[0;31m'
RESET='\e[0m'

PROMPT="${CYAN}$(basename "$0"):${RESET}"

function log_ok {
  echo -e "$PROMPT" "$@"
}

function prompt {
  echo -ne "$PROMPT" "$@"
}

function log_err {
  echo -e "$PROMPT" "${RED}Error:${RESET}" "$@"
}

function log_warn {
  echo -e "$PROMPT" "${YELLOW}Warning:${RESET}" "$@"
}

SBI_HOME=/usr/local/bin/sbi

function build_sbi {
  if ! make; then
    log_err "Failed to build sbi."
    exit 1
  fi
}

function install_sbi {
  if [ -e "$SBI_HOME" ]; then
    log_warn "sbi installation found"
    prompt "Overwrite? (y/n): "
    read -r overwrite
    if [ "$overwrite" = "y" ]; then
      log_ok "Removing previous installation..."
      sudo rm -rf "$SBI_HOME"
    else
      exit 1
    fi
  fi

  sudo cp bin/sbi "$SBI_HOME"
}

log_ok "Building sbi..."
build_sbi

log_ok "Installing..."
install_sbi

log_ok "Done!"
