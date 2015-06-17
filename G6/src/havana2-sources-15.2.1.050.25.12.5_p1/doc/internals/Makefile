all : doxygen

# The internals manuals has tentacles *everywhere* so we give up entirely
# on dependancy tracking... its too complicated.
doxygen : images
	doxygen

images :
	$(MAKE) -C ../images

clean :
	$(MAKE) -C ../images clean
	$(RM) -r html

.PHONY : doxygen images clean
