import math
import time
from array import array
import pygame

W, H = 960, 540
FPS = 60
SR = 22050

def tone(freq=220.0, dur=0.12, vol=0.3):
    n = int(SR * dur)
    buf = array("h")
    for i in range(n):
        t = i / SR
        # Simple synth: sine + 2nd harmonic + soft decay
        env = math.exp(-5.0 * t)
        s = (math.sin(2*math.pi*freq*t) + 0.35*math.sin(4*math.pi*freq*t)) * env
        buf.append(int(32767 * vol * s))
    return pygame.mixer.Sound(buffer=buf)

def main():
    pygame.mixer.pre_init(SR, -16, 1, 512)
    pygame.init()
    screen = pygame.display.set_mode((W, H))
    clock = pygame.time.Clock()

    notes = [110, 123.47, 146.83, 164.81, 196, 220, 246.94, 293.66]
    synth = [tone(f) for f in notes]
    next_note = 0
    beat = 0
    t0 = time.time()

    running = True
    while running:
        for e in pygame.event.get():
            if e.type == pygame.QUIT:
                running = False
            if e.type == pygame.KEYDOWN and e.key == pygame.K_ESCAPE:
                running = False

        t = time.time() - t0
        screen.fill((8, 8, 14))

        # Graph-like animated lissajous/plasma points
        for i in range(900):
            a = i * 0.023 + t * 1.7
            x = W * 0.5 + math.sin(a * 3.1) * 280 + math.sin(a * 0.7 + t * 2.3) * 90
            y = H * 0.5 + math.cos(a * 2.2) * 170 + math.cos(a * 1.3 - t * 1.6) * 70
            c = int(120 + 120 * math.sin(a + t * 2.0))
            pygame.draw.circle(screen, (c, 255 - c // 2, 255), (int(x), int(y)), 1)

        # Wireframe graph grid
        for gx in range(0, W, 30):
            yy = int(H * 0.72 + math.sin(gx * 0.02 + t * 2.4) * 18)
            pygame.draw.line(screen, (40, 90, 160), (gx, yy), (gx, H), 1)
        for gy in range(0, H, 28):
            xx = int(W * 0.5 + math.sin(gy * 0.03 + t * 1.8) * 60)
            pygame.draw.line(screen, (20, 45, 80), (0, gy), (W, gy), 1)

        # Sequencer (about 140 BPM, 8th notes)
        if t > next_note:
            synth[beat % len(synth)].play()
            beat += 1
            next_note = t + 60.0 / 140.0 / 2.0

        pygame.display.flip()
        clock.tick(FPS)

    pygame.quit()

if __name__ == "__main__":
    main()
