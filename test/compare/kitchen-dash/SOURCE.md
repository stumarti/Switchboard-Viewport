The kitchen panel's own firmware, unmodified, from
stumarti/reTetminal-E1002-HomeAssistant-Dash (branch sanitize-config, commit
022d133): its main.cpp (the Home Assistant fetches) and the three screens it
drew. Its fonts and icons are the firmware's own src/kd/ (byte for byte the
same files). test/compare/compare.sh builds it on the host and draws the
same Home Assistant as Switchboard Server + this firmware, pixel for pixel.
