srpm:
	tar --exclude='.git' -czvf /tmp/ghost-watch-1.0.0.tar.gz .
	mv /tmp/ghost-watch-1.0.0.tar.gz .
	rpmbuild -bs --define "_sourcedir $(CURDIR)" --define "_srcrpmdir $(outdir)" ghost-watch.spec
