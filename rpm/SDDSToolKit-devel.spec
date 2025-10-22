Summary:	SDDS database tools
Name:		SDDSToolKit-devel
License:	EPICS Open license http://www.aps.anl.gov/epics/license/open.php
Group:		Applications/Databases
URL:		https://www.aps.anl.gov/Accelerator-Operations-Physics
Packager:	Robert Soliday <soliday@aps.anl.gov>
Prefix:		/usr/lib64
Autoreq:	0
Version:	5.9
Release:	1
Source:		SDDSToolKit-devel-5.9.tar.gz

%define debug_package %{nil}
%undefine __check_files
%description
Binary package for the Self Describing Data Sets database software. 

%prep
%setup

%build
%install
mkdir -p %{buildroot}/usr/lib64/SDDS
mkdir -p %{buildroot}%{_includedir}/SDDS
install -m 644 libmdbcommon.a %{buildroot}/usr/lib64/SDDS/libmdbcommon.a
install -m 644 libmdblib.a %{buildroot}/usr/lib64/SDDS/libmdblib.a
install -m 644 libmdbmth.a %{buildroot}/usr/lib64/SDDS/libmdbmth.a
install -m 644 libSDDS1.a %{buildroot}/usr/lib64/SDDS/libSDDS1.a
install -m 644 librpnlib.a %{buildroot}/usr/lib64/SDDS/librpnlib.a
install -m 644 libfftpack.a %{buildroot}/usr/lib64/SDDS/libfftpack.a
install -m 644 libmatlib.a %{buildroot}/usr/lib64/SDDS/libmatlib.a
install -m 644 libnamelist.a %{buildroot}/usr/lib64/SDDS/libnamelist.a
install -m 644 constants.h %{buildroot}%{_includedir}/SDDS/constants.h
install -m 644 mdb.h %{buildroot}%{_includedir}/SDDS/mdb.h
install -m 644 SDDS.h %{buildroot}%{_includedir}/SDDS/SDDS.h
install -m 644 SDDStypes.h %{buildroot}%{_includedir}/SDDS/SDDStypes.h
install -m 644 namelist.h %{buildroot}%{_includedir}/SDDS/namelist.h
install -m 644 matlib.h %{buildroot}%{_includedir}/SDDS/matlib.h
install -m 644 fftpackC.h %{buildroot}%{_includedir}/SDDS/fftpackC.h

%files

/usr/lib64/SDDS/libmdbcommon.a
/usr/lib64/SDDS/libmdblib.a
/usr/lib64/SDDS/libmdbmth.a
/usr/lib64/SDDS/libSDDS1.a
/usr/lib64/SDDS/librpnlib.a
/usr/lib64/SDDS/libfftpack.a
/usr/lib64/SDDS/libmatlib.a
/usr/lib64/SDDS/libnamelist.a
%{_includedir}/SDDS/constants.h
%{_includedir}/SDDS/mdb.h
%{_includedir}/SDDS/SDDS.h
%{_includedir}/SDDS/SDDStypes.h
%{_includedir}/SDDS/namelist.h
%{_includedir}/SDDS/matlib.h
%{_includedir}/SDDS/fftpackC.h
