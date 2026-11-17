#!/usr/bin/env python3
"""Convert explicitly owner-authored/generated static PNG art to PETLRGBA v1.
No runtime PNG dependency; no automatic artwork discovery or game-data copying.
"""
import argparse
import hashlib
import json
import io
from pathlib import Path
import struct
import sys
import warnings

LABELS = {
    'press-start': (0, 576, 96),
    'new-game': (1, 336, 96),
    'continue': (2, 336, 96),
    'tutorial': (3, 336, 96),
    'ex-game': (4, 336, 96),
}
MAGIC = b'PETLRGBA'
HEADER = struct.Struct('<8s6I')

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--label', choices=LABELS, required=True, help='Exact static source-bound title label')
    parser.add_argument('--input', type=Path, required=True, help='Owner-authored/generated PNG; never original game extracts or rejected artwork')
    parser.add_argument('--output-dir', type=Path, required=True, help='Existing directory for the canonical .pelabel and provenance sidecar')
    parser.add_argument('--source-note', required=True, help='Truthful authorship/generation and approval provenance; do not include private paths')
    args = parser.parse_args(argv)
    if not args.source_note.strip():
        parser.error('--source-note must describe actual artwork provenance')
    if not args.output_dir.is_dir():
        parser.error('--output-dir must already exist')
    if not args.input.is_file() or not 0 < args.input.stat().st_size <= 4 * 1024 * 1024:
        parser.error('--input must be one PNG file of at most4MiB')
    target = args.output_dir / (args.label + '.pelabel')
    sidecar = target.with_suffix(target.suffix + '.provenance.json')
    if target.exists() or sidecar.exists():
        parser.error('Output exists; preserve it and choose a new output directory')
    try:
        import PIL
        from PIL import Image
    except ImportError:
        parser.error('Use the existing Pillow-enabled local runtime; this tool does not install dependencies')
    with args.input.open('rb') as source:
        source_png = source.read(4 * 1024 * 1024 + 1)
    if len(source_png) > 4 * 1024 * 1024:
        parser.error('Input changed or exceeds4MiB; no output was written')
    label_id, width, height = LABELS[args.label]
    try:
        with warnings.catch_warnings():
            warnings.simplefilter('error', Image.DecompressionBombWarning)
            with Image.open(io.BytesIO(source_png)) as image:
                if image.format != 'PNG' or image.size != (width, height) or getattr(image, 'n_frames', 1) != 1:
                    parser.error(f'Expected one static PNG exactly{width}x{height}; no resizing/cropping is performed')
                image.load()
                rgba = image.convert('RGBA')
                payload = rgba.tobytes('raw', 'RGBA')
    except (Image.DecompressionBombError, Image.DecompressionBombWarning) as error:
        parser.error(f'PNG rejected before conversion: {error}')
    expected = width * height * 4
    assert len(payload) == expected
    blob = HEADER.pack(MAGIC, 1, HEADER.size, width, height, expected, label_id) + payload
    assert len(blob) == 32 + expected
    provenance = {
        'format': 'PETLRGBA v1', 'label': args.label, 'labelId': label_id,
        'width': width, 'height': height, 'channels': 'straight RGBA8, top-down, tightly packed',
        'sourceNote': args.source_note.strip(), 'pillowVersion': PIL.__version__,
        'sourcePNGBytes': len(source_png),
        'sourcePNGsha256': hashlib.sha256(source_png).hexdigest(),
        'containerBytes': len(blob), 'containerSha256': hashlib.sha256(blob).hexdigest(),
        'resized': False, 'rightsOrApprovalVerifiedByTool': False,
    }
    with target.open('xb') as output:
        output.write(blob)
    with sidecar.open('x', encoding='utf-8') as output:
        json.dump(provenance, output, indent=2)
        output.write('\n')
    print(json.dumps({'file': target.name, 'bytes': len(blob), 'sha256': provenance['containerSha256']}))
    return 0

if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        print(f'Conversion failed: {error}', file=sys.stderr)
        raise SystemExit(1)
