DATE = $(shell date +%Y%m%d)

SUBDIRS = \
	addons \
	src

DIST_SUBDIRS = \
	$(SUBDIRS)

install: all
	mkdir -m 755 -p $(DESTDIR)@LIBDIR@/pvr.livestreams
	mkdir -m 755 -p $(DESTDIR)@DATADIR@/pvr.livestreams
	cp -r addons/pvr.livestreams .
	$(STRIP) -x pvr.livestreams/$(ADDONNAME)
	cp -f pvr.livestreams/$(ADDONNAME) $(DESTDIR)@LIBDIR@/pvr.livestreams ; chmod 655 $(DESTDIR)@LIBDIR@/pvr.livestreams/$(ADDONNAME)
	cp -r -f @abs_top_srcdir@/addons/pvr.livestreams/* $(DESTDIR)@DATADIR@/pvr.livestreams ; chmod -R o+rx $(DESTDIR)@DATADIR@/pvr.livestreams
	rm -Rf pvr.livestreams

uninstall:
	rm -Rf $(DESTDIR)@LIBDIR@/pvr.livestreams
	rm -Rf $(DESTDIR)@DATADIR@/pvr.livestreams

dist-zip: all
	cp -r addons/pvr.livestreams .
	cp changelog.txt pvr.livestreams
	$(STRIP) -x pvr.livestreams/$(ADDONNAME)
	zip -9 -r ./pvr.livestreams.$(VERSION).zip pvr.livestreams --exclude \*.am \*.in \*Makefile \*.gitignore
	rm -Rf pvr.livestreams

dist-zip-daily: all
	cp -r addons/pvr.livestreams .
	cp changelog.txt addons/pvr.livestreams
	cat pvr.livestreams/addon.xml | sed "s/$(VERSION)/$(VERSION).$(DATE)/g" > pvr.livestreams/addon.xml.new
	mv pvr.livestreams/addon.xml.new pvr.livestreams/addon.xml
	cp changelog.txt pvr.livestreams
if !APPLE
	$(STRIP) -s -x pvr.livestreams/$(ADDONNAME)
endif
if MINGW32
	zip -9 -r ./pvr.livestreams.$(VERSION).$(DATE).zip pvr.livestreams --exclude \*.am \*.in \*Makefile \*.gitignore
else
	zip -9 -r ./pvr.livestreams.$(VERSION).$(DATE).zip pvr.livestreams --exclude \*.am \*.in \*Makefile \*.gitignore \*.dll
endif
	rm -Rf pvr.livestreams

dist-git: all
	GIT_REV=`git --no-pager log --abbrev=7 -n 1 --pretty=format:"%h %ci" HEAD | cut -f1 -d' '` ; \
	make VERSION=$$GIT_REV dist

repo: all dist-zip
	make -C data all
	rm -Rf addon-repository
	mkdir -p addon-repository/pvr.livestreams
	cp pvr.livestreams.$(VERSION).zip addon-repository/pvr.livestreams
	cp addons/pvr.livestreams/icon.png addon-repository/pvr.livestreams
	cp data/addons.xml addon-repository
	cp data/addons.xml.md5 addon-repository

repo-daily: all dist-zip-daily
	make -C data all
	rm -Rf addon-repository
	mkdir -p addon-repository/pvr.livestreams
	cp pvr.livestreams.$(VERSION).$(DATE).zip addon-repository/pvr.livestreams
	cp addons/pvr.livestreams/icon.png addon-repository/pvr.livestreams
	cat data/addons.xml | sed "s/$(VERSION)/$(VERSION).$(DATE)/g" > addon-repository/addons.xml
	cd addon-repository && md5sum addons.xml > addons.xml.md5

release-repo-daily: repo-daily
	cp -r addon-repository/* ~/Dropbox/Public/xbmc-pvr-livestreams

repo-zip: all
	cd addons && \
	zip -9 -r ../repository.livestreamspvr.linux32.zip repository.livestreamspvr.linux32 --exclude \*.am \*.in \*Makefile \*.gitignore && \
	zip -9 -r ../repository.livestreamspvr.linux64.zip repository.livestreamspvr.linux64 --exclude \*.am \*.in \*Makefile \*.gitignore && \
	zip -9 -r ../repository.livestreamspvr.rpi.zip repository.livestreamspvr.rpi --exclude \*.am \*.in \*Makefile \*.gitignore && \
	zip -9 -r ../repository.livestreamspvr.osx.zip repository.livestreamspvr.osx --exclude \*.am \*.in \*Makefile \*.gitignore && \
	zip -9 -r ../repository.livestreamspvr.win32.zip repository.livestreamspvr.win32 --exclude \*.am \*.in \*Makefile \*.gitignore
