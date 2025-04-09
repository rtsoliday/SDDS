#!/bin/sh  
# \
exec tclsh "$0" "$@"

set dir [pwd]

set version 5.8
set name SDDSToolKit-$version
puts "Building $name RPM"

exec ./rpmdev-setuptree
exec cp -f SDDSToolKit.spec $env(HOME)/rpmbuild/SPECS/
exec rm -rf $env(HOME)/rpmbuild/BUILD/$name
exec mkdir $env(HOME)/rpmbuild/BUILD/$name
set binFiles [glob ../bin/Linux-x86_64/*]
foreach f $binFiles {
  exec chmod a+rx $f
  exec chmod a-w $f
  exec cp -f $f $env(HOME)/rpmbuild/BUILD/${name}/
}
cd $env(HOME)/rpmbuild/BUILD
exec tar -cvf ../SOURCES/${name}.tar $name
exec rm -f ../SOURCES/${name}.tar.gz
exec gzip -9 ../SOURCES/${name}.tar
cd ../SPECS
if {[catch {exec rpmbuild -bb --quiet --clean --target x86_64 \
                 --buildroot $env(HOME)/rpmbuild/BUILDROOT SDDSToolKit.spec} results]} {
}
exec rm -f ../SOURCES/${name}.tar.gz
puts $results


cd $dir
set name SDDSToolKit-devel-$version
puts "Building $name RPM"

exec cp -f SDDSToolKit-devel.spec $env(HOME)/rpmbuild/SPECS/
exec rm -rf $env(HOME)/rpmbuild/BUILD/$name
exec mkdir $env(HOME)/rpmbuild/BUILD/$name
set libFiles [glob ../lib/Linux-x86_64/*]
foreach f $libFiles {
  exec chmod a+rx $f
  exec chmod a-w $f
  exec cp -f $f $env(HOME)/rpmbuild/BUILD/${name}/
}
set incFiles [glob ../include/*.h]
foreach f $incFiles {
  exec chmod a+r $f
  exec chmod a-wx $f
  exec cp -f $f $env(HOME)/rpmbuild/BUILD/${name}/
}
cd $env(HOME)/rpmbuild/BUILD
exec tar -cvf ../SOURCES/${name}.tar $name
exec rm -f ../SOURCES/${name}.tar.gz
exec gzip -9 ../SOURCES/${name}.tar
cd ../SPECS
if {[catch {exec rpmbuild -bb --quiet --clean --target x86_64 \
                 --buildroot $env(HOME)/rpmbuild/BUILDROOT SDDSToolKit-devel.spec} results]} {
}
exec rm -f ../SOURCES/${name}.tar.gz
puts $results
puts "New RPM files in ~/rpmbuild/RPMS/x86_64"






