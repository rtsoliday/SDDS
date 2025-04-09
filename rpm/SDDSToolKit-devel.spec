Summary:	SDDS database tools
Name:		SDDSToolKit-devel
License:	EPICS Open license http://www.aps.anl.gov/epics/license/open.php
Group:		Applications/Databases
URL:		https://www.aps.anl.gov/Accelerator-Operations-Physics
Packager:	Robert Soliday <soliday@aps.anl.gov>
Prefix:		/usr/lib64
Autoreq:	0
Version:	5.8
Release:	1
Source:		SDDSToolKit-devel-5.8.tar.gz

%define debug_package %{nil}
%undefine __check_files
%description
Binary package for the Self Describing Data Sets database software. 

%prep
%setup

%build
%install
mkdir -p %{buildroot}/usr/lib64/SDDS/demo
mkdir -p %{buildroot}%{_includedir}/SDDS/lzma
install -m 644 libmdbcommon.a %{buildroot}/usr/lib64/SDDS/libmdbcommon.a
install -m 644 libmdblib.a %{buildroot}/usr/lib64/SDDS/libmdblib.a
install -m 644 libmdbmth.a %{buildroot}/usr/lib64/SDDS/libmdbmth.a
install -m 644 libSDDS1.a %{buildroot}/usr/lib64/SDDS/libSDDS1.a
install -m 644 liblzma.a %{buildroot}/usr/lib64/SDDS/liblzma.a
install -m 644 librpnlib.a %{buildroot}/usr/lib64/SDDS/librpnlib.a
install -m 644 libfftpack.a %{buildroot}/usr/lib64/SDDS/libfftpack.a
install -m 644 libmatlib.a %{buildroot}/usr/lib64/SDDS/libmatlib.a
install -m 644 constants.h %{buildroot}%{_includedir}/SDDS/constants.h
install -m 644 lzma2.h %{buildroot}%{_includedir}/SDDS/lzma.h
install -m 644 mdb.h %{buildroot}%{_includedir}/SDDS/mdb.h
install -m 644 SDDS.h %{buildroot}%{_includedir}/SDDS/SDDS.h
install -m 644 SDDStypes.h %{buildroot}%{_includedir}/SDDS/SDDStypes.h
install -m 644 base.h %{buildroot}%{_includedir}/SDDS/lzma/base.h
install -m 644 bcj.h %{buildroot}%{_includedir}/SDDS/lzma/bcj.h
install -m 644 block.h %{buildroot}%{_includedir}/SDDS/lzma/block.h
install -m 644 check.h %{buildroot}%{_includedir}/SDDS/lzma/check.h
install -m 644 container.h %{buildroot}%{_includedir}/SDDS/lzma/container.h
install -m 644 delta.h %{buildroot}%{_includedir}/SDDS/lzma/delta.h
install -m 644 filter.h %{buildroot}%{_includedir}/SDDS/lzma/filter.h
install -m 644 hardware.h %{buildroot}%{_includedir}/SDDS/lzma/hardware.h
install -m 644 index.h %{buildroot}%{_includedir}/SDDS/lzma/index.h
install -m 644 index_hash.h %{buildroot}%{_includedir}/SDDS/lzma/index_hash.h
install -m 644 lzma.h %{buildroot}%{_includedir}/SDDS/lzma/lzma.h
install -m 644 stream_flags.h %{buildroot}%{_includedir}/SDDS/lzma/stream_flags.h
install -m 644 version.h %{buildroot}%{_includedir}/SDDS/lzma/version.h
install -m 644 vli.h %{buildroot}%{_includedir}/SDDS/lzma/vli.h
install -m 644 Makefile %{buildroot}/usr/lib64/SDDS/demo/Makefile
install -m 644 sddsdemo.c %{buildroot}/usr/lib64/SDDS/demo/sddsdemo.c
install -m 644 sdds_read_demo.c %{buildroot}/usr/lib64/SDDS/demo/sdds_read_demo.c
install -m 644 sdds_write_demo.c %{buildroot}/usr/lib64/SDDS/demo/sdds_write_demo.c
install -m 644 example.sdds %{buildroot}/usr/lib64/SDDS/demo/example.sdds

%files

/usr/lib64/SDDS/libmdbcommon.a
/usr/lib64/SDDS/libmdblib.a
/usr/lib64/SDDS/libmdbmth.a
/usr/lib64/SDDS/libSDDS1.a
/usr/lib64/SDDS/liblzma.a
/usr/lib64/SDDS/librpnlib.a
/usr/lib64/SDDS/libfftpack.a
/usr/lib64/SDDS/libmatlib.a
%{_includedir}/SDDS/constants.h
%{_includedir}/SDDS/lzma.h
%{_includedir}/SDDS/mdb.h
%{_includedir}/SDDS/SDDS.h
%{_includedir}/SDDS/SDDStypes.h
%{_includedir}/SDDS/lzma/base.h
%{_includedir}/SDDS/lzma/bcj.h
%{_includedir}/SDDS/lzma/block.h
%{_includedir}/SDDS/lzma/check.h
%{_includedir}/SDDS/lzma/container.h
%{_includedir}/SDDS/lzma/delta.h
%{_includedir}/SDDS/lzma/filter.h
%{_includedir}/SDDS/lzma/hardware.h
%{_includedir}/SDDS/lzma/index.h
%{_includedir}/SDDS/lzma/index_hash.h
%{_includedir}/SDDS/lzma/lzma.h
%{_includedir}/SDDS/lzma/stream_flags.h
%{_includedir}/SDDS/lzma/version.h
%{_includedir}/SDDS/lzma/vli.h
/usr/lib64/SDDS/demo/Makefile
/usr/lib64/SDDS/demo/sddsdemo.c
/usr/lib64/SDDS/demo/sdds_read_demo.c
/usr/lib64/SDDS/demo/sdds_write_demo.c
/usr/lib64/SDDS/demo/example.sdds


