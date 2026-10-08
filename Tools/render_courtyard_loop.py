"""Render the approved courtyard still as a precisely periodic menu video.

Run with Python plus numpy, opencv-python-headless, pillow and imageio-ffmpeg.
Project-local dependencies in Saved/MenuAnimationDeps are detected automatically.
All motion uses integer harmonics of one cycle; there is no end crossfade,
camera motion, time-dependent randomness, or duplicate endpoint frame.
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
DEPS = ROOT / "Saved/MenuAnimationDeps"
if DEPS.is_dir():
    sys.path.insert(0, str(DEPS))

import cv2
import imageio_ffmpeg
import numpy as np
from PIL import Image, ImageDraw


def smoothstep(lo, hi, x):
    a = np.clip((x - lo) / (hi - lo), 0.0, 1.0)
    return a * a * (3.0 - 2.0 * a)


class CourtyardAnimation:
    def __init__(self, source: Path, width: int, height: int, duration: float):
        cv2.setNumThreads(4)
        original = cv2.imread(str(source), cv2.IMREAD_COLOR)
        if original is None:
            raise FileNotFoundError(source)
        self.source_size = [original.shape[1], original.shape[0]]
        # The supplied source is almost exactly 16:9; the scale change is <0.1%.
        self.base = cv2.resize(original, (width, height), interpolation=cv2.INTER_LANCZOS4)
        self.width, self.height, self.duration = width, height, duration
        self.scale = width / 1672.0
        yy, xx = np.mgrid[:height, :width].astype(np.float32)
        self.xx, self.yy = xx, yy
        x, y = xx / self.scale, yy * (941.0 / height)

        # Soft canopy boundary. The building, memorial, ground and camera stay fixed.
        vertices = np.array([
            (575, 0), (1560, 0), (1600, 95), (1590, 155), (1672, 175),
            (1672, 440), (1545, 480), (1450, 455), (1375, 468),
            (1270, 433), (1200, 410), (1090, 390), (980, 415),
            (875, 427), (800, 451), (700, 460), (630, 442),
            (548, 433), (456, 398), (378, 370), (364, 324),
            (391, 268), (434, 228), (423, 192), (485, 144),
            (497, 106), (549, 66),
        ], dtype=np.float32)
        vertices[:, 0] *= width / 1672.0
        vertices[:, 1] *= height / 941.0
        canopy = np.zeros((height, width), np.float32)
        cv2.fillPoly(canopy, [vertices.astype(np.int32)], 1.0)
        canopy = cv2.GaussianBlur(canopy, (0, 0), 15 * self.scale)
        trunk_anchor = np.exp(-0.5 * (((x - 1065) / 185) ** 2 + ((y - 390) / 185) ** 2))
        canopy *= (1.0 - trunk_anchor) ** 1.4
        canopy *= 1.0 - smoothstep(450, 590, y)
        self.canopy = canopy
        phase = x / 265.0 + y / 520.0
        phase2 = x / 190.0 - y / 360.0
        self.cx, self.sx = np.cos(phase), np.sin(phase)
        self.cx2, self.sx2 = np.cos(phase2), np.sin(phase2)

        # Only existing airborne leaves are animated; no new leaf artwork is added.
        b, g, r = cv2.split(self.base.astype(np.float32))
        red = ((r > 58) & (r > g * 1.50 + 8) & (r > b * 1.18 + 8)).astype(np.uint8)
        sky_vertices = np.array([
            (0, 0), (530, 0), (465, 110), (423, 181), (365, 259),
            (327, 327), (370, 395), (580, 459), (740, 456),
            (746, 520), (0, 520),
        ], dtype=np.float32)
        sky_vertices[:, 0] *= width / 1672.0
        sky_vertices[:, 1] *= height / 941.0
        sky_mask = np.zeros((height, width), np.uint8)
        cv2.fillPoly(sky_mask, [sky_vertices.astype(np.int32)], 1)
        red *= sky_mask
        count, labels, stats, centroids = cv2.connectedComponentsWithStats(red, 8)
        self.leaves = []
        self.leaf_centers = []
        for i in range(1, count):
            lx, ly, lw, lh, area = stats[i]
            if not (7 * self.scale**2 <= area <= 460 * self.scale**2):
                continue
            if max(lw, lh) > 43 * self.scale:
                continue
            center_x, center_y = centroids[i]
            # Discard tiny canopy tips near its edge and distant red architectural accents.
            if canopy[int(center_y), int(center_x)] > 0.04:
                continue
            if center_y > height * 0.50 and center_x < width * 0.10:
                continue
            if any((center_x-a)**2 + (center_y-b)**2 < (19*self.scale)**2
                   for a, b in self.leaf_centers):
                continue
            self.leaf_centers.append((float(center_x), float(center_y)))
            radius = 43 * self.scale
            x0, x1 = max(0, int(center_x-radius)), min(width, int(center_x+radius+1))
            y0, y1 = max(0, int(center_y-radius)), min(height, int(center_y+radius+1))
            d = np.sqrt(((xx[y0:y1, x0:x1]-center_x)/radius)**2
                        + ((yy[y0:y1, x0:x1]-center_y)/radius)**2)
            # Broad flat center translates the complete leaf without deforming its outline.
            weight = 1.0 - smoothstep(0.25, 1.0, d)
            leaf_phase = center_x / 137.0 + center_y / 211.0
            self.leaves.append((x0, y0, x1, y1, weight, leaf_phase))

        # Local light fluctuations respect the drawn flame and lantern window shapes.
        self.lights = []
        for index, (lx, ly, rx, ry) in enumerate([
            (105, 610, 16, 30), (366, 642, 13, 24),
            (1353, 645, 19, 25), (1600, 635, 13, 25),
            (194, 530, 4, 6), (485, 534, 4, 6),
            (635, 534, 4, 6), (751, 523, 3, 4),
            (915, 523, 4, 5), (1233, 529, 4, 5),
        ]):
            cx, cy = lx * self.scale, ly * height / 941.0
            sx, sy = rx * self.scale, ry * height / 941.0
            x0, x1 = max(0, int(cx-3*sx)), min(width, int(cx+3*sx+1))
            y0, y1 = max(0, int(cy-3*sy)), min(height, int(cy+3*sy+1))
            dist = ((xx[y0:y1,x0:x1]-cx)/sx)**2 + ((yy[y0:y1,x0:x1]-cy)/sy)**2
            falloff = np.exp(-0.5 * dist).astype(np.float32)
            patch = self.base[y0:y1,x0:x1].astype(np.float32)
            warm = ((patch[:,:,2] > 125) & (patch[:,:,1] > 65)
                    & (patch[:,:,2] > patch[:,:,0] * 1.3)).astype(np.float32)
            warm = cv2.GaussianBlur(warm, (0,0), 1.0*self.scale)
            mask = falloff * (0.18 + 0.82 * warm)
            self.lights.append((x0,y0,x1,y1,mask,index*1.618))

    def frame(self, seconds: float):
        theta = math.tau * ((seconds % self.duration) / self.duration)
        s1, c1 = math.sin(theta), math.cos(theta)-1.0
        s2, c2 = math.sin(2*theta), math.cos(2*theta)-1.0
        strength = self.canopy * self.scale
        dx = strength * (2.15*(s1*self.cx+c1*self.sx) + 0.60*(s2*self.cx2+c2*self.sx2))
        dy = strength * (0.95*(s1*self.sx-c1*self.cx) + 0.38*(s2*self.sx2-c2*self.cx2))
        for x0,y0,x1,y1,weight,phase in self.leaves:
            # An elliptical, smooth cycle gives a tiny leaf a gentle airborne drift.
            u = 7.5*self.scale*(math.sin(theta+phase)-math.sin(phase))
            v = 4.0*self.scale*(math.cos(theta+phase)-math.cos(phase))
            v += 0.85*self.scale*(math.sin(3*theta+phase)-math.sin(phase))
            dx[y0:y1,x0:x1] += u*weight
            dy[y0:y1,x0:x1] += v*weight
        frame = cv2.remap(self.base, self.xx-dx, self.yy-dy,
                          interpolation=cv2.INTER_CUBIC, borderMode=cv2.BORDER_REFLECT_101)
        for x0,y0,x1,y1,mask,phase in self.lights:
            flicker = sum(amp*(math.sin(harmonic*theta+phase)-math.sin(phase))
                          for harmonic,amp in [(7,.034),(13,.020),(23,.012),(41,.006)])
            patch = frame[y0:y1,x0:x1].astype(np.float32)
            # BGR: warm emission with a small halo, not overall image brightness pumping.
            patch += mask[:,:,None] * flicker * np.array([24,104,170], np.float32)
            frame[y0:y1,x0:x1] = np.clip(np.rint(patch),0,255).astype(np.uint8)
        return frame


def encode(scene, output: Path, fps: int):
    output.parent.mkdir(parents=True, exist_ok=True)
    count = round(scene.duration*fps)
    cmd = [imageio_ffmpeg.get_ffmpeg_exe(), '-hide_banner', '-loglevel', 'warning',
           '-y', '-f', 'rawvideo', '-pixel_format', 'bgr24', '-video_size',
           f'{scene.width}x{scene.height}', '-framerate', str(fps), '-i', 'pipe:0',
           '-an', '-c:v', 'libx264', '-preset', 'slow', '-qp', '10',
           '-x264-params', 'ipratio=1.0:pbratio=1.0:aq-mode=0:psy=0',
           '-pix_fmt', 'yuv420p', '-profile:v', 'high', '-level:v', '4.1',
           '-g', str(fps*2), '-keyint_min', str(fps*2), '-sc_threshold', '0',
           '-bf', '0', '-movflags', '+faststart', '-video_track_timescale', '30000',
           '-metadata', 'title=Crimson Courtyard - seamless menu loop', str(output)]
    started = time.monotonic()
    with subprocess.Popen(cmd, stdin=subprocess.PIPE) as proc:
        try:
            for index in range(count):
                proc.stdin.write(scene.frame(index/fps).tobytes())
                if index % (fps*2) == 0:
                    print(f'Rendered {index}/{count} frames ({time.monotonic()-started:.1f}s)', flush=True)
        finally:
            proc.stdin.close()
        if proc.wait() != 0:
            raise RuntimeError('FFmpeg failed')


def verify(scene, video: Path, fps: int, asset_dir: Path, prefix='CrimsonCourtyard'):
    first, end = scene.frame(0), scene.frame(scene.duration)
    period_error = int(np.abs(first.astype(np.int16)-end.astype(np.int16)).max())
    assert period_error == 0, 'Animation did not return exactly to its initial state'
    capture = cv2.VideoCapture(str(video))
    assert capture.isOpened(), 'Encoded video did not open'
    expected = round(scene.duration*fps)
    previous = None
    initial = None
    last = None
    steps = []
    count = 0
    darkest_mean = 255.0
    saved = {}
    checkpoints = {0, expected//4, expected//2, expected*3//4, expected-1}
    while True:
        okay, frame = capture.read()
        if not okay:
            break
        small = cv2.resize(frame, (480,270), interpolation=cv2.INTER_AREA).astype(np.float32)
        if initial is None:
            initial = small
        if previous is not None:
            steps.append(float(np.abs(small-previous).mean()))
        previous = small
        last = small
        darkest_mean = min(darkest_mean, float(small.mean()))
        if count in checkpoints:
            saved[count] = frame
        count += 1
    properties = dict(width=int(capture.get(cv2.CAP_PROP_FRAME_WIDTH)),
                      height=int(capture.get(cv2.CAP_PROP_FRAME_HEIGHT)),
                      fps=capture.get(cv2.CAP_PROP_FPS))
    capture.release()
    assert count == expected, f'Decoded {count}, expected {expected}'
    assert darkest_mean > 10, 'Unexpected black frame'
    wrap_difference = float(np.abs(last-initial).mean())
    median_step = float(np.median(steps))
    # Compression can change adjacent-frame error slightly at the first keyframe.
    assert wrap_difference < max(np.percentile(steps,99)*1.5, 0.25), 'Visible wrap discontinuity'
    report = dict(**properties, duration_seconds=scene.duration, frames=count,
                  codec='H.264', pixel_format='yuv420p', audio=False,
                  source_size=scene.source_size, file_bytes=video.stat().st_size,
                  period_max_pixel_error=period_error,
                  decoded_wrap_mean_absolute_difference=wrap_difference,
                  decoded_median_adjacent_difference=median_step,
                  decoded_99th_percentile_adjacent_difference=float(np.percentile(steps,99)),
                  darkest_frame_mean=darkest_mean,
                  animated_existing_leaves=len(getattr(scene, 'sprites', scene.leaves)),
                  description=getattr(scene, 'description', 'Fixed camera; subtle periodic canopy sway, existing leaf drift and lantern flicker. No crossfade or duplicate endpoint.'))
    (asset_dir/f'{prefix}_Loop_Validation.json').write_text(json.dumps(report,indent=2)+'\n')
    cv2.imwrite(str(asset_dir/f'{prefix}_Poster.jpg'), first, [cv2.IMWRITE_JPEG_QUALITY,95])
    sheet = Image.new('RGB',(960, 3*292), '#101a22')
    draw = ImageDraw.Draw(sheet)
    for index,(frame_number,frame) in enumerate(sorted(saved.items())):
        thumb = cv2.cvtColor(cv2.resize(frame,(480,270)),cv2.COLOR_BGR2RGB)
        x, y = (index%2)*480, (index//2)*292
        sheet.paste(Image.fromarray(thumb),(x,y))
        draw.text((x+8,y+274),f'{frame_number/fps:.3f} s', fill='#e7d4b5')
    sheet.save(asset_dir/f'{prefix}_ContactSheet.jpg',quality=94)
    print(json.dumps(report,indent=2),flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--width',type=int,default=1920)
    parser.add_argument('--height',type=int,default=1080)
    parser.add_argument('--duration',type=float,default=16)
    parser.add_argument('--fps',type=int,default=30)
    parser.add_argument('--output',type=Path,default=ROOT/'Content/Movies/CrimsonCourtyard_Loop.mp4')
    args = parser.parse_args()
    asset_dir = ROOT/'Art/MenuBackground'
    scene = CourtyardAnimation(asset_dir/'CrimsonCourtyard_Source.png',args.width,args.height,args.duration)
    print(f'Animating {len(scene.leaves)} existing airborne leaves; source {scene.source_size}',flush=True)
    encode(scene,args.output,args.fps)
    verify(scene,args.output,args.fps,asset_dir)


if __name__ == '__main__':
    main()
