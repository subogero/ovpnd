install:
	cp ovpnd $(DESTDIR)/usr/bin/
	sed '/^exit 0/ i /usr/bin/ovpnd' -i $(DESTDIR)/etc/rc.local
	$(DESTDIR)/usr/bin/ovpnd
uninstall:
	$(DESTDIR)/usr/bin/ovpnd q
	sed '/ovpnd/ d' -i $(DESTDIR)/etc/rc.local
	rm $(DESTDIR)/usr/bin/ovpnd
