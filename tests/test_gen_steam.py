"""tools/gen_steam.py: struct_sizes() gives the pack(4) size of the plain Steam structures
copied as is between the guest (i386) and Steam (arm64). Synthetic Steamworks.NET-style
sources, so that no SDK download is needed."""
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import gen_steam  # noqa: E402

AUTOGEN = 'Assets/com.rlabrecque.steamworks.net/Runtime/autogen/'
CONSTANTS = '\t\tpublic const int k_cchTitle = 128 + 1;\n'
STRUCTS = '''
\tpublic struct SteamUGCDetails_t {
\t\tpublic ulong m_nPublishedFileId;
\t\tpublic EResult m_eResult;
\t\t[MarshalAs(UnmanagedType.ByValArray, SizeConst = Constants.k_cchTitle)]
\t\tprivate byte[] m_rgchTitle_;
\t\tpublic ulong m_ulSteamIDOwner;\t\t// aligned on 4, not 8, under pack(4)
\t\t[MarshalAs(UnmanagedType.I1)]
\t\tpublic bool m_bBanned;
\t\tpublic CSteamID m_steamID;
\t\tpublic float m_flScore;
\t\t[MarshalAs(UnmanagedType.I1)]
\t\tpublic bool m_bLast;
\t}
'''


class StructSizes(unittest.TestCase):
    def test_pack4_layout(self):
        with tempfile.TemporaryDirectory() as tmp:
            files = {}
            for name, text in (('SteamConstants.cs', CONSTANTS), ('SteamStructs.cs', STRUCTS)):
                path = Path(tmp) / name
                path.write_text(text)
                files[AUTOGEN + name] = path
            sizes = gen_steam.struct_sizes(files, {'EResult': 'int'})
        # 8 + 4 + 129 = 141 -> 144, + 8 = 152, + 1 = 153 -> 156, + 8 + 4 + 1 = 169, rounded to 172.
        self.assertEqual(sizes, {'SteamUGCDetails_t': 172})


if __name__ == '__main__':
    unittest.main()
