all: sky-websense

CONTIKI=../../..

APPS += webbrowser
APPS += powertrace
APPS += collect-view

MODULES += core/net/http-socket

CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
CONTIKI_SOURCEFILES += wget.c
PROJECTDIRS += ../rpl-border-router
PROJECT_SOURCEFILES += httpd-simple.c
PROJECT_SOURCEFILES += collect-common.c

CONTIKI_WITH_IPV6 = 1
CONTIKI_WITH_RIME = 1
include $(CONTIKI)/Makefile.include

$(CONTIKI)/tools/tunslip6:	$(CONTIKI)/tools/tunslip6.c
	(cd $(CONTIKI)/tools && $(MAKE) tunslip6)

connect-router:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 aaaa::1/64

connect-router-cooja:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 -a 127.0.0.1 aaaa::1/64
