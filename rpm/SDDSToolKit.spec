Summary:	SDDS database tools
Name:		SDDSToolKit
License:	EPICS Open license http://www.aps.anl.gov/epics/license/open.php
Group:		Applications/Databases
URL:		https://www.aps.anl.gov/Accelerator-Operations-Physics
Packager:	Robert Soliday <soliday@aps.anl.gov>
Prefix:		%{_bindir}
Autoreq:	0
Version:	5.9
Release:	1
Source:		SDDSToolKit-5.9.tar.gz

%define debug_package %{nil}
%undefine __check_files
%description
Binary package for the Self Describing Data Sets database software. 

%prep
%setup

%build
%install
mkdir -p %{buildroot}%{_bindir}
install -s -m 755 citi2sdds %{buildroot}%{_bindir}/citi2sdds
install -s -m 755 csv2sdds %{buildroot}%{_bindir}/csv2sdds
install -s -m 755 editstring %{buildroot}%{_bindir}/editstring
install -s -m 755 elegant2genesis %{buildroot}%{_bindir}/elegant2genesis
install -s -m 755 hdf2sdds %{buildroot}%{_bindir}/hdf2sdds
install -s -m 755 if2pf %{buildroot}%{_bindir}/if2pf
install -s -m 755 image2sdds %{buildroot}%{_bindir}/image2sdds
install -s -m 755 lba2sdds %{buildroot}%{_bindir}/lba2sdds
install -s -m 755 mcs2sdds %{buildroot}%{_bindir}/mcs2sdds
install -s -m 755 mpl2sdds %{buildroot}%{_bindir}/mpl2sdds
install -m 755 mpl_qt %{buildroot}%{_bindir}/mpl_qt
install -s -m 755 nlpp %{buildroot}%{_bindir}/nlpp
install -s -m 755 plaindata2sdds %{buildroot}%{_bindir}/plaindata2sdds
install -s -m 755 raw2sdds %{buildroot}%{_bindir}/raw2sdds
install -s -m 755 replaceText %{buildroot}%{_bindir}/replaceText
install -s -m 755 rpn %{buildroot}%{_bindir}/rpn
install -s -m 755 rpnl %{buildroot}%{_bindir}/rpnl
install -s -m 755 sdds2dfft %{buildroot}%{_bindir}/sdds2dfft
install -s -m 755 sdds2dinterpolate %{buildroot}%{_bindir}/sdds2dinterpolate
install -s -m 755 sdds2dpfit %{buildroot}%{_bindir}/sdds2dpfit
install -s -m 755 sdds2hdf %{buildroot}%{_bindir}/sdds2hdf
install -s -m 755 sdds2math %{buildroot}%{_bindir}/sdds2math
install -s -m 755 sdds2mpl %{buildroot}%{_bindir}/sdds2mpl
install -s -m 755 sdds2plaindata %{buildroot}%{_bindir}/sdds2plaindata
install -s -m 755 sdds2spreadsheet %{buildroot}%{_bindir}/sdds2spreadsheet
install -s -m 755 sdds2stream %{buildroot}%{_bindir}/sdds2stream
install -s -m 755 sdds2tiff %{buildroot}%{_bindir}/sdds2tiff
install -s -m 755 sddsarray2column %{buildroot}%{_bindir}/sddsarray2column
install -s -m 755 sddsbaseline %{buildroot}%{_bindir}/sddsbaseline
install -s -m 755 sddsbinarystring %{buildroot}%{_bindir}/sddsbinarystring
install -s -m 755 sddsbreak %{buildroot}%{_bindir}/sddsbreak
install -s -m 755 sddscast %{buildroot}%{_bindir}/sddscast
install -s -m 755 sddschanges %{buildroot}%{_bindir}/sddschanges
install -s -m 755 sddscheck %{buildroot}%{_bindir}/sddscheck
install -s -m 755 sddscliptails %{buildroot}%{_bindir}/sddscliptails
install -s -m 755 sddscollapse %{buildroot}%{_bindir}/sddscollapse
install -s -m 755 sddscollect %{buildroot}%{_bindir}/sddscollect
install -s -m 755 sddscombine %{buildroot}%{_bindir}/sddscombine
install -s -m 755 sddscombinelogfiles %{buildroot}%{_bindir}/sddscombinelogfiles
install -s -m 755 sddscongen %{buildroot}%{_bindir}/sddscongen
install -s -m 755 sddscontour %{buildroot}%{_bindir}/sddscontour
install -s -m 755 sddsconvert %{buildroot}%{_bindir}/sddsconvert
install -s -m 755 sddsconvertalarmlog %{buildroot}%{_bindir}/sddsconvertalarmlog
install -s -m 755 sddsconvertlogonchange %{buildroot}%{_bindir}/sddsconvertlogonchange
install -s -m 755 sddsconvolve %{buildroot}%{_bindir}/sddsconvolve
install -s -m 755 sddscorrelate %{buildroot}%{_bindir}/sddscorrelate
install -s -m 755 sddsderef %{buildroot}%{_bindir}/sddsderef
install -s -m 755 sddsderiv %{buildroot}%{_bindir}/sddsderiv
install -s -m 755 sddsdiff %{buildroot}%{_bindir}/sddsdiff
install -s -m 755 sddsdigfilter %{buildroot}%{_bindir}/sddsdigfilter
install -s -m 755 sddsdistest %{buildroot}%{_bindir}/sddsdistest
install -s -m 755 sddsduplicate %{buildroot}%{_bindir}/sddsduplicate
install -s -m 755 sddsendian %{buildroot}%{_bindir}/sddsendian
install -s -m 755 sddsenvelope %{buildroot}%{_bindir}/sddsenvelope
install -s -m 755 sddseventhist %{buildroot}%{_bindir}/sddseventhist
install -s -m 755 sddsexpand %{buildroot}%{_bindir}/sddsexpand
install -s -m 755 sddsexpfit %{buildroot}%{_bindir}/sddsexpfit
install -s -m 755 sddsfdfilter %{buildroot}%{_bindir}/sddsfdfilter
install -s -m 755 sddsfft %{buildroot}%{_bindir}/sddsfft
install -s -m 755 sddsgenericfit %{buildroot}%{_bindir}/sddsgenericfit
install -s -m 755 sddsgfit %{buildroot}%{_bindir}/sddsgfit
install -s -m 755 sddslorentzianfit %{buildroot}%{_bindir}/sddslorentzianfit
install -s -m 755 sddshist %{buildroot}%{_bindir}/sddshist
install -s -m 755 sddshist2d %{buildroot}%{_bindir}/sddshist2d
install -s -m 755 sddsimageconvert %{buildroot}%{_bindir}/sddsimageconvert
install -s -m 755 sddsimageprofiles %{buildroot}%{_bindir}/sddsimageprofiles
install -s -m 755 sddsinsideboundaries %{buildroot}%{_bindir}/sddsinsideboundaries
install -s -m 755 sddsinteg %{buildroot}%{_bindir}/sddsinteg
install -s -m 755 sddsinterp %{buildroot}%{_bindir}/sddsinterp
install -s -m 755 sddsinterpset %{buildroot}%{_bindir}/sddsinterpset
install -s -m 755 sddskde %{buildroot}%{_bindir}/sddskde
install -s -m 755 sddskde2d %{buildroot}%{_bindir}/sddskde2d
install -s -m 755 sddslocaldensity %{buildroot}%{_bindir}/sddslocaldensity
install -s -m 755 sddsmakedataset %{buildroot}%{_bindir}/sddsmakedataset
install -s -m 755 sddsmatrixmult %{buildroot}%{_bindir}/sddsmatrixmult
install -s -m 755 sddsmatrixop %{buildroot}%{_bindir}/sddsmatrixop
install -s -m 755 sddsmatrix2column %{buildroot}%{_bindir}/sddsmatrix2column
install -s -m 755 sddsminterp %{buildroot}%{_bindir}/sddsminterp
install -s -m 755 sddsmpfit %{buildroot}%{_bindir}/sddsmpfit
install -s -m 755 sddsmselect %{buildroot}%{_bindir}/sddsmselect
install -s -m 755 sddsmultihist %{buildroot}%{_bindir}/sddsmultihist
install -s -m 755 sddsmxref %{buildroot}%{_bindir}/sddsmxref
install -s -m 755 sddsnaff %{buildroot}%{_bindir}/sddsnaff
install -s -m 755 sddsnormalize %{buildroot}%{_bindir}/sddsnormalize
install -s -m 755 sddsoutlier %{buildroot}%{_bindir}/sddsoutlier
install -s -m 755 sddspeakfind %{buildroot}%{_bindir}/sddspeakfind
install -s -m 755 sddspfit %{buildroot}%{_bindir}/sddspfit
install -m 755 sddsplot %{buildroot}%{_bindir}/sddsplot
install -s -m 755 sddspoly %{buildroot}%{_bindir}/sddspoly
install -s -m 755 sddsprintout %{buildroot}%{_bindir}/sddsprintout
install -s -m 755 sddsprocess %{buildroot}%{_bindir}/sddsprocess
install -s -m 755 sddspseudoinverse %{buildroot}%{_bindir}/sddspseudoinverse
install -s -m 755 sddsquery %{buildroot}%{_bindir}/sddsquery
install -s -m 755 sddsregroup %{buildroot}%{_bindir}/sddsregroup
install -s -m 755 sddsrespmatrixderivative %{buildroot}%{_bindir}/sddsrespmatrixderivative
install -s -m 755 sddsrowstats %{buildroot}%{_bindir}/sddsrowstats
install -s -m 755 sddsrunstats %{buildroot}%{_bindir}/sddsrunstats
install -s -m 755 sddssampledist %{buildroot}%{_bindir}/sddssampledist
install -s -m 755 sddsselect %{buildroot}%{_bindir}/sddsselect
install -s -m 755 sddsseparate %{buildroot}%{_bindir}/sddsseparate
install -s -m 755 sddssequence %{buildroot}%{_bindir}/sddssequence
install -s -m 755 sddsshift %{buildroot}%{_bindir}/sddsshift
install -s -m 755 sddsshiftcor %{buildroot}%{_bindir}/sddsshiftcor
install -s -m 755 sddssinefit %{buildroot}%{_bindir}/sddssinefit
install -s -m 755 sddsslopes %{buildroot}%{_bindir}/sddsslopes
install -s -m 755 sddssmooth %{buildroot}%{_bindir}/sddssmooth
install -s -m 755 sddssnap2grid %{buildroot}%{_bindir}/sddssnap2grid
install -s -m 755 sddssort %{buildroot}%{_bindir}/sddssort
install -s -m 755 sddssortcolumn %{buildroot}%{_bindir}/sddssortcolumn
install -s -m 755 sddssplit %{buildroot}%{_bindir}/sddssplit
install -s -m 755 sddsspotanalysis %{buildroot}%{_bindir}/sddsspotanalysis
install -s -m 755 sddstimeconvert %{buildroot}%{_bindir}/sddstimeconvert
install -s -m 755 sddstranspose %{buildroot}%{_bindir}/sddstranspose
install -s -m 755 sddstdrpeeling %{buildroot}%{_bindir}/sddstdrpeeling
install -s -m 755 sddsunwrap %{buildroot}%{_bindir}/sddsunwrap
install -s -m 755 sddsvslopes %{buildroot}%{_bindir}/sddsvslopes
install -s -m 755 sddsxref %{buildroot}%{_bindir}/sddsxref
install -s -m 755 sddszerofind %{buildroot}%{_bindir}/sddszerofind
install -s -m 755 tek2sdds %{buildroot}%{_bindir}/tek2sdds
install -s -m 755 TFS2sdds %{buildroot}%{_bindir}/TFS2sdds
install -s -m 755 tiff2sdds %{buildroot}%{_bindir}/tiff2sdds
install -s -m 755 mecho %{buildroot}%{_bindir}/mecho
install -s -m 755 minpath %{buildroot}%{_bindir}/minpath
install -s -m 755 tmpname %{buildroot}%{_bindir}/tmpname
install -s -m 755 token %{buildroot}%{_bindir}/token
install -s -m 755 timeconvert %{buildroot}%{_bindir}/timeconvert
install -s -m 755 tcomp %{buildroot}%{_bindir}/tcomp
install -s -m 755 wfm2sdds %{buildroot}%{_bindir}/wfm2sdds

%files

%{_bindir}/citi2sdds
%{_bindir}/csv2sdds
%{_bindir}/editstring
%{_bindir}/elegant2genesis
%{_bindir}/hdf2sdds
%{_bindir}/if2pf
%{_bindir}/image2sdds
%{_bindir}/lba2sdds
%{_bindir}/mcs2sdds
%{_bindir}/mpl2sdds
%{_bindir}/mpl_qt
%{_bindir}/nlpp
%{_bindir}/plaindata2sdds
%{_bindir}/raw2sdds
%{_bindir}/replaceText
%{_bindir}/rpn
%{_bindir}/rpnl
%{_bindir}/sdds2dfft
%{_bindir}/sdds2dinterpolate
%{_bindir}/sdds2dpfit
%{_bindir}/sdds2hdf
%{_bindir}/sdds2math
%{_bindir}/sdds2mpl
%{_bindir}/sdds2plaindata
%{_bindir}/sdds2spreadsheet
%{_bindir}/sdds2stream
%{_bindir}/sdds2tiff
%{_bindir}/sddsarray2column
%{_bindir}/sddsbaseline
%{_bindir}/sddsbinarystring
%{_bindir}/sddsbreak
%{_bindir}/sddscast
%{_bindir}/sddschanges
%{_bindir}/sddscheck
%{_bindir}/sddscliptails
%{_bindir}/sddscollapse
%{_bindir}/sddscollect
%{_bindir}/sddscombine
%{_bindir}/sddscombinelogfiles
%{_bindir}/sddscongen
%{_bindir}/sddscontour
%{_bindir}/sddsconvert
%{_bindir}/sddsconvertlogonchange
%{_bindir}/sddsconvolve
%{_bindir}/sddscorrelate
%{_bindir}/sddsderef
%{_bindir}/sddsderiv
%{_bindir}/sddsdiff
%{_bindir}/sddsdigfilter
%{_bindir}/sddsdistest
%{_bindir}/sddsduplicate
%{_bindir}/sddsendian
%{_bindir}/sddsenvelope
%{_bindir}/sddseventhist
%{_bindir}/sddsexpand
%{_bindir}/sddsexpfit
%{_bindir}/sddsfdfilter
%{_bindir}/sddsfft
%{_bindir}/sddsgenericfit
%{_bindir}/sddsgfit
%{_bindir}/sddslorentzianfit
%{_bindir}/sddshist
%{_bindir}/sddshist2d
%{_bindir}/sddsimageconvert
%{_bindir}/sddsimageprofiles
%{_bindir}/sddsinsideboundaries
%{_bindir}/sddsinteg
%{_bindir}/sddsinterp
%{_bindir}/sddsinterpset
%{_bindir}/sddskde
%{_bindir}/sddskde2d
%{_bindir}/sddslocaldensity
%{_bindir}/sddsmakedataset
%{_bindir}/sddsmatrixmult
%{_bindir}/sddsmatrixop
%{_bindir}/sddsmatrix2column
%{_bindir}/sddsminterp
%{_bindir}/sddsmpfit
%{_bindir}/sddsmselect
%{_bindir}/sddsmultihist
%{_bindir}/sddsmxref
%{_bindir}/sddsnaff
%{_bindir}/sddsnormalize
%{_bindir}/sddsoutlier
%{_bindir}/sddspeakfind
%{_bindir}/sddspfit
%{_bindir}/sddsplot
%{_bindir}/sddspoly
%{_bindir}/sddsprintout
%{_bindir}/sddsprocess
%{_bindir}/sddspseudoinverse
%{_bindir}/sddsquery
%{_bindir}/sddsregroup
%{_bindir}/sddsrespmatrixderivative
%{_bindir}/sddsrowstats
%{_bindir}/sddsrunstats
%{_bindir}/sddssampledist
%{_bindir}/sddsselect
%{_bindir}/sddsseparate
%{_bindir}/sddssequence
%{_bindir}/sddsshift
%{_bindir}/sddsshiftcor
%{_bindir}/sddssinefit
%{_bindir}/sddsslopes
%{_bindir}/sddssmooth
%{_bindir}/sddssnap2grid
%{_bindir}/sddssort
%{_bindir}/sddssortcolumn
%{_bindir}/sddssplit
%{_bindir}/sddsspotanalysis
%{_bindir}/sddstimeconvert
%{_bindir}/sddstranspose
%{_bindir}/sddstdrpeeling
%{_bindir}/sddsunwrap
%{_bindir}/sddsvslopes
%{_bindir}/sddsxref
%{_bindir}/sddszerofind
%{_bindir}/tek2sdds
%{_bindir}/TFS2sdds
%{_bindir}/tiff2sdds
%{_bindir}/mecho
%{_bindir}/minpath
%{_bindir}/tmpname
%{_bindir}/token
%{_bindir}/timeconvert
%{_bindir}/tcomp
%{_bindir}/wfm2sdds



