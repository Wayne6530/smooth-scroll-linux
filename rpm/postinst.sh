#!/bin/sh

systemctl daemon-reload
systemctl enable smooth-scroll.service
systemctl start smooth-scroll.service
