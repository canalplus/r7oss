Name:       linux-src
Summary:    STB linux source
Version:    0
Release:    0
Group:      System/Kernel
License:    see LICENSE in source
Source0:    %{name}-%{version}.tar.gz
Source100:  %{name}-rpmlintrc

AutoReqProv: no
BuildArch:   noarch

%description
This is the linux source for STB.

%prep
%setup -q

%build
echo "... nothing to build ..."

%install
install -d %{buildroot}/opt/stb-src/linux
cp -pr * %{buildroot}/opt/stb-src/linux
rm -rf %{buildroot}/opt/stb-src/linux/packaging

%files
%defattr(-,root,root,-)
/opt/stb-src/linux
