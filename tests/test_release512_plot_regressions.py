"""Editor and plotting regressions found while reviewing changes after SDDS-5.12."""

import platform

import pytest

from test_audit_fix_build import query


@pytest.mark.skipif(platform.system() not in ("Linux", "Darwin"), reason="Unix makefile queries")
@pytest.mark.parametrize("legacy_source", ("environment", "command-line"))
@pytest.mark.parametrize("explicit_version", (False, True))
def test_windows_editor_legacy_qt_major_override(tmp_path, monkeypatch, legacy_source, explicit_version):
  """A legacy Qt-major setting must not form a cycle with the selected Qt version."""
  monkeypatch.delenv("QT_VERSION", raising=False)
  monkeypatch.delenv("QT_MAJOR", raising=False)
  arguments = ["OS=Windows", "ARCH=x86_64", "QT_BUILD_GOALS=", "HAVE_QT=1"]
  if legacy_source == "environment":
    monkeypatch.setenv("QT_MAJOR", "6")
  else:
    arguments.append("QT_MAJOR=6")
  if explicit_version:
    arguments.append("QT_VERSION=5")
  values = query("SDDSaps/sddseditor", tmp_path, ("QT_VERSION", "QT_MAJOR"), *arguments)
  assert values["QT_VERSION"] == ("5" if explicit_version else "6")
  assert values["QT_MAJOR"] == ("6" if legacy_source == "command-line" else values["QT_VERSION"])
