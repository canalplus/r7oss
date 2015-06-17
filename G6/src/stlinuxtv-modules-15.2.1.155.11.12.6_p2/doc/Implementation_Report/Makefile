SOURCE = ImplementationReport.txt

all: pdf html cleanup

pdf:
	a2x -L -f pdf -d book $(SOURCE)

html:
	asciidoc $(SOURCE)

cleanup:
	rm -rf *.png
	rm -rf *.fo
	rm -rf *.xml

clean:
	rm -rf *.png
	rm -rf *.fo
	rm -rf *.xml
	rm -rf *.pdf
	rm -rf *.html
