Summary: Compiles information about file transfers from logfiles
Name: xferstats
Version: 2.16
Release: 28%{?dist}
URL: http://xferstats.off.net/
Source0: ftp://xferstats.off.net/%{name}-%{version}.tar.gz
Patch0: xferstats.patch
Patch1: xferstats-2.16-config-loc.patch
Patch2: xferstats-2.16-display.patch
Patch3: xferstats-glib2.patch
Patch4: xferstats-2.16-man.patch
License: GPLv2+
Group: Applications/System

BuildRequires: autoconf, glib2-devel

%description
xferstats compiles information about file transfers from logfiles.

%prep
%setup -q

%patch0 -p1
%patch1 -p1 -b .config
%patch2 -p1 -b .display
%patch3 -p1 -b .glib2
%patch4 -p1 -b .man

%build
sed -e "s,PKG_PROG_PKG_CONFIG.*,PKG_CONFIG=%{_bindir}/pkg-config," %{_datadir}/aclocal/glib-2.0.m4 >aclocal.m4
autoconf
%configure

make %{?_smp_mflags}

%install
%make_install DESTDIR=%{buildroot}

mkdir -p %{buildroot}%{_datadir}/xferstats/
cp -a graphs %{buildroot}%{_datadir}/xferstats/

%files
%doc ChangeLog
%config(noreplace) %{_sysconfdir}/xferstats.cfg
%{_bindir}/xferstats
%{_mandir}/*/*
%{_datadir}/xferstats

%changelog
* Fri Jan 24 2014 Daniel Mach <dmach@redhat.com> - 2.16-28
- Mass rebuild 2014-01-24

* Fri Dec 27 2013 Daniel Mach <dmach@redhat.com> - 2.16-27
- Mass rebuild 2013-12-27

* Thu Apr 04 2013 Jan Synáček <jsynacek@redhat.com> - 2.16-26
- Add missing option to manpage

* Fri Feb 15 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.16-25
- Rebuilt for https://fedoraproject.org/wiki/Fedora_19_Mass_Rebuild

* Wed Aug 22 2012 Jan Synáček <jsynacek@redhat.com> - 2.16-24
- Fix changelog entry
- Make even more fedora-review friendly

* Sun Jul 22 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.16-23
- Rebuilt for https://fedoraproject.org/wiki/Fedora_18_Mass_Rebuild

* Thu Jul 19 2012 Jan Synáček <jsynacek@redhat.com> - 2.16-23
- Make fedora-review friendly

* Thu Jan 05 2012 Jan Synáček <jsynacek@redhat.com> - 2.16-22
- Rebuilt for GCC 4.7

* Mon Feb 07 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.16-21
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Mon Jul 27 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.16-20
- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild

* Thu Feb 26 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.16-19
- Rebuilt for https://fedoraproject.org/wiki/Fedora_11_Mass_Rebuild

* Tue Aug 12 2008 Jason L Tibbitts III <tibbs@math.uh.edu> - 2.16-18
- Fix license tag.

* Tue Feb 19 2008 Fedora Release Engineering <rel-eng@fedoraproject.org> - 2.16-17
- Autorebuild for GCC 4.3

* Tue Jan 08 2008 Than Ngo <than@redhat.com> 2.16-16
- rebuilt

* Thu Oct 18 2007  2.16-15 Than Ngo <than@redhat.com> 2.16-15
- rebuild

* Wed Jul 12 2006 Jesse Keating <jkeating@redhat.com> - 2.16-14.1
- rebuild

* Tue Apr 14 2006 Bill Nottingham <notting@redhat.com> - 2.16-14
- build against glib2

* Fri Feb 10 2006 Jesse Keating <jkeating@redhat.com> - 2.16-13.2.1
- bump again for double-long bug on ppc(64)

* Tue Feb 07 2006 Jesse Keating <jkeating@redhat.com> - 2.16-13.2
- rebuilt for new gcc4.1 snapshot and glibc changes

* Fri Dec 09 2005 Jesse Keating <jkeating@redhat.com>
- rebuilt

* Wed Mar 16 2005 Than Ngo <than@redhat.com> 2.16-13
- rebuilt against gcc 4

* Wed Feb 09 2005 Than Ngo <than@redhat.com> 2.16-12
- rebuilt

* Tue Jun 15 2004 Elliot Lee <sopwith@redhat.com>
- rebuilt

* Thu May 20 2004 Than Ngo <than@redhat.com> 2.16-10
- add BuildRequires on glib-devel, bug #123761

* Fri Feb 13 2004 Elliot Lee <sopwith@redhat.com>
- rebuilt

* Wed Jun 04 2003 Elliot Lee <sopwith@redhat.com>
- rebuilt

* Tue Jun  3 2003 Than Ngo <than@redhat.com> 2.16-7
- add a patch file from Gilbert, which fixes incorrect line
  counting for output reports #bug 91852

* Wed May 28 2003 Than Ngo <than@redhat.com> 2.16-6
- add mising "completed transfers" column from Gilbert E. Detillieux

* Wed Jan 22 2003 Tim Powers <timp@redhat.com>
- rebuilt

* Wed Dec 11 2002 Tim Powers <timp@redhat.com> 2.16-4
- rebuild on all arches

* Thu Jul 11 2002 Preston Brown <pbrown@redhat.com>
- rebuild for mainline distro inclusion

* Mon Apr 23 2001 Tim Powers <timp@redhat.com>
- fix where xferstats is expecting a config file, was in
/usr/local/etc, fixed to point to /etc (bug #36559)

* Thu Dec 14 2000 Tim Powers <timp@redhat.com>
- Initial build.
- there aren't any docs outside of the Changelog for this, well, the
  INSTALL file is there but that doesn't apply to the package at all

