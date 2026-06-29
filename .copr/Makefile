srpm:
	tar --exclude='./.git' --exclude='./ghost-watch-1.0.0.tar.gz' -czvf ghost-watch-1.0.0.tar.gz .
	rpmbuild -bs --define "_sourcedir $(CURDIR)" --define "_srcrpmdir $(outdir)" ghost-watch.spec
