"""Standalone 480x320 PHYSBOX IMU puck visual prototype.

Run with ``python prototype/imu_ball.py`` after installing pygame.  Arrow keys
or WASD simulate board tilt; Space resets the puck and Escape exits.
"""

from __future__ import annotations

import math

import pygame


SCREEN_SIZE = (480, 320)
FPS = 60

PLOT = pygame.Rect(10, 10, 310, 300)
PANEL_X = 340
PUCK_RADIUS = 12

BALL_ACCEL_SCALE = 420.0
BALL_DAMPING = 2.15
BALL_BOUNCE = 0.58
BALL_MAX_SPEED = 310.0
IMU_FILTER_ALPHA = 0.14
IMU_DEADZONE = 0.025

BACKGROUND = (247, 249, 252)
PLOT_BACKGROUND = (255, 255, 255)
GRID_MINOR = (219, 229, 240)
GRID_AXIS = (91, 145, 203)
BORDER = (168, 190, 215)
TEXT = (37, 54, 72)
TEXT_MUTED = (105, 123, 142)
PUCK = (207, 77, 58)
PUCK_DARK = (159, 52, 42)
PUCK_LIGHT = (237, 127, 108)


def apply_deadzone(value: float) -> float:
    if abs(value) <= IMU_DEADZONE:
        return 0.0
    return math.copysign(
        (abs(value) - IMU_DEADZONE) / (1.0 - IMU_DEADZONE), value
    )


def draw_plot(surface: pygame.Surface, small_font: pygame.font.Font) -> None:
    pygame.draw.rect(surface, PLOT_BACKGROUND, PLOT, border_radius=7)

    origin_x, origin_y = PLOT.center
    spacing = 30
    for x in range(origin_x, PLOT.right, spacing):
        pygame.draw.line(surface, GRID_MINOR, (x, PLOT.top), (x, PLOT.bottom - 1))
    for x in range(origin_x - spacing, PLOT.left, -spacing):
        pygame.draw.line(surface, GRID_MINOR, (x, PLOT.top), (x, PLOT.bottom - 1))
    for y in range(origin_y, PLOT.bottom, spacing):
        pygame.draw.line(surface, GRID_MINOR, (PLOT.left, y), (PLOT.right - 1, y))
    for y in range(origin_y - spacing, PLOT.top, -spacing):
        pygame.draw.line(surface, GRID_MINOR, (PLOT.left, y), (PLOT.right - 1, y))

    pygame.draw.line(surface, GRID_AXIS, (origin_x, PLOT.top), (origin_x, PLOT.bottom - 1), 2)
    pygame.draw.line(surface, GRID_AXIS, (PLOT.left, origin_y), (PLOT.right - 1, origin_y), 2)
    pygame.draw.rect(surface, BORDER, PLOT, 1, border_radius=7)

    for label, position in (
        ("-", (PLOT.left + 6, origin_y + 4)),
        ("+X", (PLOT.right - 27, origin_y + 4)),
        ("+Y", (origin_x + 5, PLOT.top + 4)),
        ("0", (origin_x + 5, origin_y + 4)),
    ):
        surface.blit(small_font.render(label, True, TEXT_MUTED), position)


def draw_puck(surface: pygame.Surface, position: pygame.Vector2) -> None:
    center = (round(position.x), round(position.y))
    pygame.draw.circle(surface, (185, 194, 205), (center[0] + 2, center[1] + 3), PUCK_RADIUS + 1)
    pygame.draw.circle(surface, PUCK_DARK, center, PUCK_RADIUS)
    pygame.draw.circle(surface, PUCK, center, PUCK_RADIUS - 1)
    pygame.draw.circle(surface, PUCK_LIGHT, (center[0] - 4, center[1] - 4), 3)


def draw_vector_group(
    surface: pygame.Surface,
    title_font: pygame.font.Font,
    value_font: pygame.font.Font,
    title: str,
    y: int,
    values: tuple[float, float, float],
    unit: str,
) -> None:
    surface.blit(title_font.render(title, True, TEXT_MUTED), (PANEL_X, y))
    for index, (axis, value) in enumerate(zip("XYZ", values)):
        line_y = y + 21 + index * 22
        surface.blit(value_font.render(axis, True, GRID_AXIS), (PANEL_X, line_y))
        rendered = value_font.render(f"{value:+.2f} {unit}", True, TEXT)
        surface.blit(rendered, (PANEL_X + 22, line_y))


def main() -> None:
    pygame.init()
    pygame.display.set_caption("PHYSBOX IMU Puck Prototype")
    screen = pygame.display.set_mode(SCREEN_SIZE)
    clock = pygame.time.Clock()
    title_font = pygame.font.SysFont("segoeui", 13, bold=True)
    value_font = pygame.font.SysFont("consolas", 14)
    small_font = pygame.font.SysFont("segoeui", 11)

    position = pygame.Vector2(PLOT.center)
    velocity = pygame.Vector2()
    filtered_accel = pygame.Vector2()
    previous_input = pygame.Vector2()
    gyro = pygame.Vector2()
    running = True

    while running:
        dt = min(clock.tick(FPS) / 1000.0, 0.05)
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    running = False
                elif event.key == pygame.K_SPACE:
                    position.update(PLOT.center)
                    velocity.update(0.0, 0.0)

        keys = pygame.key.get_pressed()
        input_accel = pygame.Vector2(
            float(keys[pygame.K_RIGHT] or keys[pygame.K_d])
            - float(keys[pygame.K_LEFT] or keys[pygame.K_a]),
            float(keys[pygame.K_DOWN] or keys[pygame.K_s])
            - float(keys[pygame.K_UP] or keys[pygame.K_w]),
        )
        if input_accel.length_squared() > 1.0:
            input_accel.normalize_ip()

        filtered_accel += (input_accel - filtered_accel) * IMU_FILTER_ALPHA
        filtered_accel.update(
            apply_deadzone(filtered_accel.x), apply_deadzone(filtered_accel.y)
        )
        if dt > 0.0:
            gyro += (((input_accel - previous_input) / dt) - gyro) * 0.12
        previous_input = input_accel

        velocity += filtered_accel * BALL_ACCEL_SCALE * dt
        velocity *= math.exp(-BALL_DAMPING * dt)
        if velocity.length() > BALL_MAX_SPEED:
            velocity.scale_to_length(BALL_MAX_SPEED)
        position += velocity * dt

        min_x, max_x = PLOT.left + PUCK_RADIUS, PLOT.right - PUCK_RADIUS - 1
        min_y, max_y = PLOT.top + PUCK_RADIUS, PLOT.bottom - PUCK_RADIUS - 1
        if position.x < min_x or position.x > max_x:
            position.x = max(min_x, min(position.x, max_x))
            velocity.x *= -BALL_BOUNCE
        if position.y < min_y or position.y > max_y:
            position.y = max(min_y, min(position.y, max_y))
            velocity.y *= -BALL_BOUNCE

        screen.fill(BACKGROUND)
        draw_plot(screen, small_font)
        draw_puck(screen, position)
        draw_vector_group(
            screen,
            title_font,
            value_font,
            "ACCELERATION",
            23,
            (filtered_accel.x, filtered_accel.y, 1.0),
            "g",
        )
        draw_vector_group(
            screen,
            title_font,
            value_font,
            "GYROSCOPE",
            132,
            (gyro.x, gyro.y, 0.0),
            "d/s",
        )
        screen.blit(small_font.render("ARROWS / WASD  tilt", True, TEXT_MUTED), (PANEL_X, 250))
        screen.blit(small_font.render("SPACE  reset", True, TEXT_MUTED), (PANEL_X, 267))
        screen.blit(small_font.render(f"{clock.get_fps():4.0f} FPS", True, TEXT_MUTED), (PANEL_X, 291))
        pygame.display.flip()

    pygame.quit()


if __name__ == "__main__":
    main()
