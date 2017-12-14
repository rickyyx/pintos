BUILD_SUBDIRS = threads userprog vm filesys

push: clean
	rsync -avz --no-perms ./ chen11@corn.stanford.edu:~/private/pintos/src/

pull:
	rsync -avz --no-perms chen11@corn.stanford.edu:~/private/pintos/src/ ./

# push: clean
# 	rsync -avz --no-perms ./ chen11@rice.stanford.edu:~/afs-home/private/pintos/src/
# 
# pull:
# 	rsync -avz --no-perms chen11@rice.stanford.edu:~/afs-home/private/pintos/src/ ./

all::
	@echo "Run 'make' in subdirectories: $(BUILD_SUBDIRS)."
	@echo "This top-level make has only 'clean' targets."

CLEAN_SUBDIRS = $(BUILD_SUBDIRS) examples utils

clean::
	for d in $(CLEAN_SUBDIRS); do $(MAKE) -C $$d $@; done

distclean:: clean
	find . -name '*~' -exec rm '{}' \;

TAGS_SUBDIRS = $(BUILD_SUBDIRS) devices lib
TAGS_SOURCES = find $(TAGS_SUBDIRS) -name \*.[chS] -print

TAGS::
	etags --members `$(TAGS_SOURCES)`

tags::
	ctags -T --no-warn `$(TAGS_SOURCES)`

cscope.files::
	$(TAGS_SOURCES) > cscope.files

cscope:: cscope.files
	cscope -b -q -k
