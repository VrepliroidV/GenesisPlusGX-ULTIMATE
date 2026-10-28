#---------------------------------------------------------------------------------
# 32X Test: Wii front end for the PicoDrive libretro core
# The core is built first (see .github/workflows/build.yml):
#   make -C picodrive -f Makefile.libretro platform=wii STATIC_LINKING=0
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

include $(DEVKITPPC)/wii_rules

TARGET		:=	32xtest
BUILD		:=	build
SOURCES		:=	source
INCLUDES	:=	source

export PICODRIVE ?= $(CURDIR)/picodrive

CFLAGS	=	-O2 -Wall $(MACHDEP) $(INCLUDE) -DHW_RVL \
		-I$(PICODRIVE)/platform/libretro/libretro-common/include
LDFLAGS	=	$(MACHDEP) -Wl,-Map,$(notdir $@).map

LIBS	:=	$(PICODRIVE)/picodrive_libretro_wii.a -lfat -lwiiuse -lbte -logc -lm
LIBDIRS	:=	$(PORTLIBS)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
export OFILES	:=	$(CFILES:.c=.o)
export LD	:=	$(CC)

export INCLUDE	:=	$(foreach dir,$(INCLUDES), -iquote $(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD) \
			-I$(LIBOGC_INC)

export LIBPATHS	:=	-L$(LIBOGC_LIB) $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).dol

else

DEPENDS	:=	$(OFILES:.o=.d)

$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
