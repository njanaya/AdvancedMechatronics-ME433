#to run git Bash and run "py -3.12 -m pgzero brick_breaker.py"
import os
import serial
os.environ['SDL_VIDEO_CENTERED'] = "100,100"

import pgzrun
from pgzero.keyboard import keyboard
from pgzero.constants import keys

WIDTH = 800
HEIGHT = 600

paddle = Rect((WIDTH // 2 - 60, HEIGHT - 40), (120, 15))
ball = Rect((WIDTH // 2 - 10, HEIGHT // 2), (20, 20))

ball_vx = 4
ball_vy = -4

score = 0
lives = 3
game_over = False
win = False
game_started = False
ball_launched = False

bricks = []

ser = None

try:
    ser = serial.Serial("COM8", 115200, timeout=0)
    print("Connected to Pico on COM8")
except:
    print("No Pico found. Using keyboard control.")


def make_bricks():
    global bricks
    bricks = []

    rows = 5
    cols = 10
    brick_w = 70
    brick_h = 25
    gap = 8
    start_x = 25
    start_y = 70

    for row in range(rows):
        for col in range(cols):
            x = start_x + col * (brick_w + gap)
            y = start_y + row * (brick_h + gap)
            bricks.append(Rect((x, y), (brick_w, brick_h)))


def reset_ball():
    global ball_vx, ball_vy, ball_launched

    ball.x = paddle.x + paddle.width // 2 - ball.width // 2
    ball.y = paddle.y - ball.height - 2

    ball_vx = 4
    ball_vy = -4
    ball_launched = False


def reset_game():
    global score, lives, game_over, win, game_started

    score = 0
    lives = 3
    game_over = False
    win = False
    game_started = True

    paddle.x = WIDTH // 2 - 60
    make_bricks()
    reset_ball()

def read_pico():
    if ser is None:
        return None, None

    try:
        line = ser.readline().decode().strip()

        if line:
            parts = line.split(",")

            if len(parts) == 2:
                tilt_value = int(parts[0])
                button_value = int(parts[1])

                return tilt_value, button_value

    except:
        pass

    return None, None

make_bricks()
reset_ball()


def update():
    global ball_vx, ball_vy, score, lives, game_over, win
    global ball_launched, game_started

    if not game_started:

        tilt_value, button_value = read_pico()

        if keyboard[keys.SPACE] or button_value == 1:
            game_started = True
            reset_ball()
        return

    if game_over or win:
        tilt_value, button_value = read_pico()

        if keyboard[keys.SPACE] or button_value == 1:
            reset_game()

        return

    tilt_value, button_value = read_pico()

    if tilt_value is not None:

        # Adjust after testing
        tilt_min = -12000
        tilt_max = 12000

        tilt_value = max(
            tilt_min,
            min(tilt_max, tilt_value)
        )

        paddle.x = int(
            (tilt_value - tilt_min)
            * (WIDTH - paddle.width)
            / (tilt_max - tilt_min)
        )
    else:
        if keyboard[keys.LEFT]:
            paddle.x -= 7

        if keyboard[keys.RIGHT]:
            paddle.x += 7

    paddle.x = max(0, min(WIDTH - paddle.width, paddle.x))

    if not ball_launched:
        ball.x = paddle.x + paddle.width // 2 - ball.width // 2
        ball.y = paddle.y - ball.height - 2

        if keyboard[keys.SPACE] or button_value == 1:
            ball_launched = True
        return

    ball.x += ball_vx
    ball.y += ball_vy

    if ball.left <= 0 or ball.right >= WIDTH:
        ball_vx *= -1

    if ball.top <= 0:
        ball_vy *= -1

    if ball.colliderect(paddle) and ball_vy > 0:
        ball_vy *= -1

        paddle_center = paddle.x + paddle.width / 2
        ball_center = ball.x + ball.width / 2
        offset = (ball_center - paddle_center) / (paddle.width / 2)
        ball_vx = 6 * offset

    for brick in bricks[:]:
        if ball.colliderect(brick):
            bricks.remove(brick)
            ball_vy *= -1
            score += 1
            break

    if ball.top > HEIGHT:
        lives -= 1
        if lives <= 0:
            game_over = True
        else:
            reset_ball()

    if len(bricks) == 0:
        win = True


def draw():
    screen.clear()
    screen.fill((15, 15, 30))

    if not game_started:
        screen.draw.text(
            "Nick Anaya's Brick Breaker",
            center=(WIDTH // 2, HEIGHT // 2 - 100),
            fontsize=70,
            color="white"
        )

        screen.draw.text(
            "LEFT / RIGHT = Move Paddle",
            center=(WIDTH // 2, HEIGHT // 2),
            fontsize=35,
            color="cyan"
        )

        screen.draw.text(
            "SPACE = Launch Ball",
            center=(WIDTH // 2, HEIGHT // 2 + 45),
            fontsize=35,
            color="yellow"
        )

        screen.draw.text(
            "Press SPACE to Start",
            center=(WIDTH // 2, HEIGHT // 2 + 120),
            fontsize=45,
            color="white"
        )

        return

    screen.draw.text("Brick Breaker", (20, 15), fontsize=40, color="white")
    screen.draw.text(f"Score: {score}", (590, 20), fontsize=30, color="yellow")
    screen.draw.text(f"Lives: {lives}", (700, 20), fontsize=30, color="red")

    screen.draw.filled_rect(paddle, "cyan")
    screen.draw.filled_rect(ball, "white")

    colors = ["red", "orange", "yellow", "green", "blue"]

    for i, brick in enumerate(bricks):
        row = i // 10
        screen.draw.filled_rect(brick, colors[row % len(colors)])

    if game_over:
        screen.draw.text(
            "GAME OVER",
            center=(WIDTH // 2, HEIGHT // 2 - 30),
            fontsize=70,
            color="white"
        )
        screen.draw.text(
            "Press SPACE to restart",
            center=(WIDTH // 2, HEIGHT // 2 + 40),
            fontsize=35,
            color="white"
        )

    if win:
        screen.draw.text(
            "YOU WIN!",
            center=(WIDTH // 2, HEIGHT // 2 - 30),
            fontsize=70,
            color="white"
        )
        screen.draw.text(
            "Press SPACE to restart",
            center=(WIDTH // 2, HEIGHT // 2 + 40),
            fontsize=35,
            color="white"
        )


pgzrun.go()