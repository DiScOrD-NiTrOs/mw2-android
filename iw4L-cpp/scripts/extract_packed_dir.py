import re, pathlib

text = pathlib.Path("iw4L/crates/entity_iw4/src/glass_packed_dir.rs").read_text(encoding="utf-8")

# Pull every f32::from_bits(...) as a hex word.
words = re.findall(r"f32::from_bits\((0x[0-9a-fA-F]+)\)", text)
if len(words) % 3 != 0:
    raise SystemExit(f"unexpected word count {len(words)} (not a multiple of 3)")

out = pathlib.Path("iw4L-cpp/src/entity_iw4/glass_packed_dir_data.inc")
lines = []
for i in range(0, len(words), 3):
    def to_f32(x):
        v = int(x, 16)
        import struct
        return struct.pack("<f", v).hex()  # placeholder; we instead emit the hex literal as f suffix
    # Emit as C++ hex-float literal. Use the exact bits as a hexfloat to preserve bits.
    h0, h1, h2 = words[i], words[i+1], words[i+2]
    lines.append(f"0x{h0[2:]}p+0f, 0x{h1[2:]}p+0f, 0x{h2[2:]}p+0f,\n")
out.write_text("".join(lines), encoding="utf-8")
print("wrote", len(lines), "rows; total words", len(words))
