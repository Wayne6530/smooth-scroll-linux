#!/bin/sh

if [ "$1" = "0" ]; then
    systemctl stop smooth-scroll.service || true
    systemctl disable smooth-scroll.service || true
    systemctl daemon-reload
fi
