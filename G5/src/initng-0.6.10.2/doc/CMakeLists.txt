SET(DOC_FILES
	gentoo-chart.png
	initng-chart.png
	initng.txt
)

SET(MAN8_FILES
        initng.8 
	ngc.8
        ngdc.8
)
				
INSTALL_FILES(${DATA_INSTALL_DIR}/doc/initng FILES ${DOC_FILES})
INSTALL_FILES(${DATA_INSTALL_DIR}/man/man8 FILES ${MAN8_FILES})
