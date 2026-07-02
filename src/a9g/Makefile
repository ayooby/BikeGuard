## ----------------------------------------------------------- ##
## Don't touch the next line unless you know what you are doing.##
## ----------------------------------------------------------- ##
LOCAL_NAME := demo/motion_test
LOCAL_MODULE_DEPENDS += init
LOCAL_MODULE_DEPENDS += libs
LOCAL_API_DEPENDS := libs/gps \
                     libs/utils
LOCAL_ADD_INCLUDE := include\
                    include/std_inc \
                    include/api_inc \
                    libs/gps/include \
                    libs/gps/minmea/src
IS_ENTRY_POINT := no

## ------------------------------------ ##
##    Add your custom flags here       ##
## ------------------------------------ ##
MYCFLAGS += 

## ------------------------------------- ##
##    List all your sources here       ##
## ------------------------------------- ##
C_SRC := ${notdir ${wildcard src/*.c}}

## ------------------------------------- ##
##  Do Not touch below this line       ##
## ------------------------------------- ##
include ${SOFT_WORKDIR}/platform/compilation/cust_rules.mk
