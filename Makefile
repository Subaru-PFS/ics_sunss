install:
	if test `pi/usr/local/bin/sysstate` != "normal"; then \
	    echo "warning: / is not writeable; reboot with reboot_n to make permanent changes"; \
		echo "... will continue in 3s.... "; sleep 3; \
	fi
	sudo cp -pr pi/usr/local/bin/* /usr/local/bin/
	
install_system:
	sudo cp -pr etc/* /etc/

