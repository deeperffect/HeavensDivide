"""More animated companion to the subtle courtyard loop.

The existing airborne leaves become individually composited moving layers.
Their hidden background is reconstructed locally for the animation, then each
leaf drifts downwind and flutters. Smooth zero-opacity ends hide the path reset.
All animation frequencies complete an integer number of cycles in 16 seconds.
The original subtle MP4 and preview remain available for comparison.
"""

import argparse
import math
from pathlib import Path

from render_courtyard_loop import (
    ROOT, CourtyardAnimation, cv2, encode, np, smoothstep, verify,
)


class ActiveCourtyardAnimation(CourtyardAnimation):
    description = (
        'Fixed camera; stronger canopy sway with two wind cycles; existing leaves '
        'drift downwind, rotate and flutter with invisible path resets; warmer '
        'lantern flicker. Every layer repeats over 16 seconds. No full-image crossfade.'
    )

    def __init__(self, source, width=1920, height=1080, duration=16):
        super().__init__(source, width, height, duration)
        self.original = self.base.copy()
        b, g, r = cv2.split(self.original.astype(np.float32))
        red = ((r > 58) & (r > g*1.50+8) & (r > b*1.18+8)).astype(np.uint8)
        count, labels, stats, centers = cv2.connectedComponentsWithStats(red, connectivity=8)
        valid = np.flatnonzero((stats[:,cv2.CC_STAT_AREA] >= 7*self.scale**2)
                               & (stats[:,cv2.CC_STAT_AREA] <= 700*self.scale**2))
        valid = valid[valid != 0]
        self.sprites = []
        removed = np.zeros((height,width),np.uint8)
        selected = set()
        dilation = max(1,round(2*self.scale))
        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE,(2*dilation+1,2*dilation+1))
        for index,(cx,cy) in enumerate(self.leaf_centers):
            distances = np.sum((centers[valid]-[cx,cy])**2,axis=1)
            label = int(valid[np.argmin(distances)])
            if float(distances.min()) > (6*self.scale)**2 or label in selected:
                continue
            selected.add(label)
            lx,ly,lw,lh,area = stats[label]
            pad = dilation+5
            x0,y0 = max(0,lx-pad),max(0,ly-pad)
            x1,y1 = min(width,lx+lw+pad),min(height,ly+lh+pad)
            shape = (labels[y0:y1,x0:x1] == label).astype(np.uint8)*255
            shape = cv2.dilate(shape,kernel)
            removed[y0:y1,x0:x1] = np.maximum(removed[y0:y1,x0:x1],shape)
            alpha = cv2.GaussianBlur(shape.astype(np.float32)/255.0,(0,0),0.50*self.scale)
            pixels = self.original[y0:y1,x0:x1].astype(np.float32)
            rgba = np.dstack([pixels*alpha[:,:,None],alpha])
            self.sprites.append(dict(
                image=rgba, cx=float(cx), cy=float(cy),
                pivot=(float(cx-x0),float(cy-y0)),
                offset=(index*0.618033988749895+0.137)%1.0,
                cycles=2 if index%3 else 1,
                drift_x=-(110+(index*37)%105)*self.scale,
                drift_y=(45+(index*29)%75)*self.scale,
                phase=index*1.73,
            ))
        assert len(self.sprites) >= 15, 'Too few source leaves could be isolated cleanly'
        repair_mask = cv2.dilate(removed,np.ones((3,3),np.uint8))
        self.base = cv2.inpaint(self.original,repair_mask,5.0*self.scale,cv2.INPAINT_TELEA)
        self.removal_mask = repair_mask
        # Stronger movement remains localized to the outer crown, away from the trunk.
        self.canopy *= 3.2
        self.leaves = []
        self.lights = [(x0,y0,x1,y1,mask*1.85,phase)
                       for x0,y0,x1,y1,mask,phase in self.lights]

    def draw_leaf(self, frame, leaf, t):
        progress = (t*leaf['cycles']+leaf['offset'])%1.0
        opacity = float(smoothstep(0.0,0.12,progress)*(1.0-smoothstep(0.86,1.0,progress)))
        if opacity < 0.0001:
            return
        phase = leaf['phase']
        phase_t = math.tau*progress
        cx = leaf['cx']+leaf['drift_x']*(progress-.5)
        cx += 9*self.scale*math.sin(2*phase_t+phase)
        cy = leaf['cy']+leaf['drift_y']*(progress-.5)
        cy += 8*self.scale*math.sin(phase_t+phase)
        angle = math.radians(25*math.sin(2*phase_t+phase)+12*math.cos(phase_t-phase))
        # The leaf turns slightly edge-on as it tumbles, without hard flips.
        sx = .64+.36*(.5+.5*math.cos(3*phase_t+phase))
        sy = .97+.06*math.sin(2*phase_t-phase)
        cosine,sine = math.cos(angle),math.sin(angle)
        matrix = np.array([[cosine*sx,-sine*sy],[sine*sx,cosine*sy]],np.float32)
        ph,pw = leaf['image'].shape[:2]
        radius = int(math.ceil(math.hypot(pw,ph)))+4
        ox,oy = int(math.floor(cx-radius/2)),int(math.floor(cy-radius/2))
        affine = np.zeros((2,3),np.float32)
        affine[:,:2] = matrix
        affine[:,2] = [cx-ox,cy-oy]-matrix@np.array(leaf['pivot'],np.float32)
        rotated = cv2.warpAffine(leaf['image'],affine,(radius,radius),
                                 flags=cv2.INTER_CUBIC,borderMode=cv2.BORDER_CONSTANT)
        x0,y0 = max(0,ox),max(0,oy)
        x1,y1 = min(self.width,ox+radius),min(self.height,oy+radius)
        if x0>=x1 or y0>=y1:
            return
        patch = rotated[y0-oy:y1-oy,x0-ox:x1-ox]
        alpha = np.clip(patch[:,:,3:4],0,1)*opacity
        # Premultiplied alpha avoids dark fringes on the small rotated leaf outlines.
        color = np.clip(patch[:,:,:3],0,255)*opacity
        region = frame[y0:y1,x0:x1].astype(np.float32)
        frame[y0:y1,x0:x1] = np.clip(np.rint(region*(1-alpha)+color),0,255).astype(np.uint8)

    def frame(self, seconds):
        # Two complete wind cycles per movie; the leaf paths complete one or two.
        frame = super().frame(seconds*2.0)
        t = (seconds%self.duration)/self.duration
        for leaf in self.sprites:
            self.draw_leaf(frame,leaf,t)
        return frame


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--width',type=int,default=1920)
    parser.add_argument('--height',type=int,default=1080)
    parser.add_argument('--fps',type=int,default=30)
    parser.add_argument('--diagnostics-only',action='store_true')
    args = parser.parse_args()
    asset_dir = ROOT/'Art/MenuBackground'
    scene = ActiveCourtyardAnimation(asset_dir/'CrimsonCourtyard_Source.png',args.width,args.height)
    print(f'Animated variant: {len(scene.sprites)} drifting leaf layers; 3.2x canopy movement.',flush=True)
    diagnostic_dir = ROOT/'Saved/MenuAnimationQA/Active'
    diagnostic_dir.mkdir(parents=True,exist_ok=True)
    for t in [0,2,4,8,12,15.9666666667]:
        cv2.imwrite(str(diagnostic_dir/f'frame_{t:.3f}.jpg'),scene.frame(t),[cv2.IMWRITE_JPEG_QUALITY,95])
    cv2.imwrite(str(diagnostic_dir/'clean_plate.jpg'),scene.base,[cv2.IMWRITE_JPEG_QUALITY,95])
    if args.diagnostics_only:
        print('Diagnostic frames saved.',flush=True)
        return
    output = ROOT/'Content/Movies/CrimsonCourtyard_Animated_Loop.mp4'
    encode(scene,output,args.fps)
    verify(scene,output,args.fps,asset_dir,prefix='CrimsonCourtyard_Animated')


if __name__ == '__main__':
    main()
