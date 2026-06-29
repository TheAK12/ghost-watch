srpm:
	git archive --format=tar.gz -o ghost-watch-1.0.0.tar.gz HEAD
	rpmbuild -bs --define "_sourcedir $(PWD)" --define "_srcrpmdir $(outdir)" ghost-watch.spec
