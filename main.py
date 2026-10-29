import pygame
import random
import os

# pygame setup
pygame.init()
screen = pygame.display.set_mode((800, 600))
pygame.display.set_caption("Car Collector")
clock = pygame.time.Clock()
running = True
dt = 0
game_over_sound_played = False

# Game variables
score = 0
speed = 300 

# Game States: "MENU", "PLAYING", "PAUSED", "GAME_OVER"
game_state = "MENU"

# Set up positions for coins
coin_pos = pygame.Vector2(random.randint(40, 760), random.randint(40, 560))
time_pos = pygame.Vector2(random.randint(40, 760), random.randint(40, 560))
bonus_pos = pygame.Vector2(random.randint(40, 760), random.randint(40, 560))

# Load original image asset with convert_alpha() for transparency support
playerImageBase = pygame.image.load("assets/car.png").convert_alpha()
playerImageBase = pygame.transform.scale(playerImageBase, (25, 45))

# load the image asset for the coin
coin_img = pygame.image.load("assets/coin.png").convert_alpha()
coin_img = pygame.transform.scale(coin_img, (40, 40))

time_img = pygame.image.load("assets/jerry can.png").convert_alpha()
time_img = pygame.transform.scale(time_img, (40, 40))

bonus_img = pygame.image.load("assets/gem.gif").convert_alpha()
bonus_img = pygame.transform.scale(bonus_img, (40, 40))

playerImage = playerImageBase
player_rect = playerImage.get_rect(center=(screen.get_width() / 2, screen.get_height() / 2))

# Sub-pixel position tracker
pos_x = float(player_rect.centerx)
pos_y = float(player_rect.centery)

# --- AUDIO SETUP ---
pygame.mixer.init()

pygame.mixer.music.load("assets/music1.mp3") 
pygame.mixer.music.set_volume(0.4) 
pygame.mixer.music.play(-1)        

jerry_can_sound = pygame.mixer.Sound("assets/ding.wav")
jerry_can_sound.set_volume(0.6)

coin_sound = pygame.mixer.Sound("assets/coin.wav")
coin_sound.set_volume(0.6)

bonus_sound = pygame.mixer.Sound("assets/bonus coin sound.wav")
bonus_sound.set_volume(0.6)

game_over_sound = pygame.mixer.Sound("assets/game over.wav")
game_over_sound.set_volume(0.6)

# --- FONT & TIMER SETUP ---
pygame.font.init()
game_font = pygame.font.SysFont("Arial", 30, bold=True)
title_font = pygame.font.SysFont("Arial", 50, bold=True)

time_left = 30.0 

# --- INITIAL HIGH SCORE LOAD ---
if os.path.exists("highscore.txt"):
    with open("highscore.txt", "r") as file:
        content = file.read().strip()
        high_score = int(content) if content.isdigit() else 0
else:
    high_score = 0

# --- JERRY CAN SPAWN TIMER ---
time_visible = False         
jerry_respawn_timer = 0.0   

# --- RUBY SPAWN TIMER ---
ruby_time_visible = False
ruby_spawn_timer = random.uniform(3.0, 6.0)

def reset_game():
    global score, time_left, pos_x, pos_y, game_over_sound_played
    global coin_pos, bonus_pos, time_pos, ruby_time_visible, time_visible
    global ruby_spawn_timer, jerry_respawn_timer

    score = 0
    time_left = 30.0
    pos_x, pos_y = screen.get_width() / 2, screen.get_height() / 2
    game_over_sound_played = False

    coin_pos = pygame.Vector2(random.randint(40, 760), random.randint(40, 560))
    time_pos = pygame.Vector2(900, 900)
    bonus_pos = pygame.Vector2(900, 900)

    time_visible = False
    ruby_time_visible = False
    jerry_respawn_timer = random.uniform(5.0, 10.0)
    ruby_spawn_timer = random.uniform(3.0, 6.0)

    game_over_sound.stop()
    pygame.mixer.music.play(-1)

while running:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False

        if event.type == pygame.KEYDOWN:
            if game_state == "MENU":
                if event.key in (pygame.K_SPACE, pygame.K_RETURN):
                    reset_game()
                    game_state = "PLAYING"

            elif game_state == "PLAYING":
                if event.key in (pygame.K_p, pygame.K_ESCAPE):
                    game_state = "PAUSED"
                    pygame.mixer.music.pause()

            elif game_state == "PAUSED":
                if event.key in (pygame.K_p, pygame.K_ESCAPE, pygame.K_SPACE):
                    game_state = "PLAYING"
                    pygame.mixer.music.unpause()

            elif game_state == "GAME_OVER":
                if event.key == pygame.K_r:
                    reset_game()
                    game_state = "PLAYING"

    # --- MAIN MENU STATE ---
    if game_state == "MENU":
        screen.fill((30, 30, 45))
        title_txt = title_font.render("CAR COLLECTOR", True, "cyan")
        start_txt = game_font.render("Press SPACE or ENTER to Start", True, "white")
        hs_txt = game_font.render(f"High Score: {high_score}", True, "yellow")

        screen.blit(title_txt, (title_txt.get_rect(center=(400, 200))))
        screen.blit(start_txt, (start_txt.get_rect(center=(400, 300))))
        screen.blit(hs_txt, (hs_txt.get_rect(center=(400, 360))))

        pygame.display.flip()
        dt = clock.tick(60) / 1000
        continue

    # --- GAME OVER STATE ---
    if game_state == "GAME_OVER":
        screen.fill("black")
        over_txt = game_font.render("GAME OVER", True, "red")
        final_txt = game_font.render(f"Final Score: {score}", True, "white")
        hs_txt = game_font.render(f"High Score: {high_score}", True, "yellow")
        restart_txt = game_font.render("Press R to Restart", True, "gray")

        screen.blit(over_txt, (320, 200))
        screen.blit(final_txt, (310, 250))
        screen.blit(hs_txt, (310, 290))
        screen.blit(restart_txt, (290, 360))

        if not game_over_sound_played:
            pygame.mixer.music.pause()
            game_over_sound.play()
            game_over_sound_played = True

        pygame.display.flip()
        dt = clock.tick(60) / 1000
        continue

    # --- PLAYING STATE UPDATES (ONLY EXECUTES WHEN UNPAUSED) ---
    if game_state == "PLAYING":
        time_left -= dt
        if time_left <= 0:
            time_left = 0
            game_state = "GAME_OVER"

        keys = pygame.key.get_pressed()
        
        # Calculate movement speeds
        move_speed = speed * dt
        diagonal_speed = (speed * 0.707) * dt

        # Handle Input, Rotations, and Move coordinates
        if keys[pygame.K_w] and keys[pygame.K_d]:
            playerImage = pygame.transform.rotate(playerImageBase, 135.0)
            pos_y -= diagonal_speed
            pos_x += diagonal_speed
        elif keys[pygame.K_w] and keys[pygame.K_a]:
            playerImage = pygame.transform.rotate(playerImageBase, 225.0)
            pos_y -= diagonal_speed
            pos_x -= diagonal_speed
        elif keys[pygame.K_s] and keys[pygame.K_d]:
            playerImage = pygame.transform.rotate(playerImageBase, 45.0)
            pos_y += diagonal_speed
            pos_x += diagonal_speed
        elif keys[pygame.K_s] and keys[pygame.K_a]:
            playerImage = pygame.transform.rotate(playerImageBase, 315.0)
            pos_y += diagonal_speed
            pos_x -= diagonal_speed
        elif keys[pygame.K_w]:
            playerImage = pygame.transform.rotate(playerImageBase, 180.0)
            pos_y -= move_speed
        elif keys[pygame.K_s]:
            playerImage = pygame.transform.rotate(playerImageBase, 0.0)
            pos_y += move_speed
        elif keys[pygame.K_a]:
            playerImage = pygame.transform.rotate(playerImageBase, 270.0)
            pos_x -= move_speed
        elif keys[pygame.K_d]:
            playerImage = pygame.transform.rotate(playerImageBase, 90.0)
            pos_x += move_speed

        player_rect.centerx = int(pos_x)
        player_rect.centery = int(pos_y)

        # Screen boundary checks
        if player_rect.right > 760:
            pos_x -= speed * dt
        if player_rect.left < 40:
            pos_x += speed * dt
        if player_rect.bottom > 560:
            pos_y -= speed * dt
        if player_rect.top < 40:
            pos_y += speed * dt
            
        player_rect.centerx = int(pos_x)
        player_rect.centery = int(pos_y)

        player_center = pygame.Vector2(player_rect.center)
        
        # Coin collection
        if player_center.distance_to(coin_pos) < 40:    
            coin_pos.x = random.randint(40, 760)
            coin_pos.y = random.randint(40, 560)
            coin_sound.play()

            score += 1
            str_score = str(score)
            
            if score > high_score: 
                high_score = score
                with open("highscore.txt", "w") as file:
                    file.write(str_score)

        # Gem / Ruby spawn & collection logic
        if not ruby_time_visible:
            ruby_spawn_timer -= dt
            bonus_pos.x = 900
            bonus_pos.y = 900
            if ruby_spawn_timer <= 0:
                bonus_pos.x = random.randint(40, 760)
                bonus_pos.y = random.randint(40, 560)
                ruby_time_visible = True

        if player_center.distance_to(bonus_pos) < 40:    
            bonus_sound.play()
            ruby_spawn_timer = random.uniform(3.0, 6.0)
            ruby_time_visible = False

            score += 5
            str_score = str(score)

            if score > high_score: 
                high_score = score
                with open("highscore.txt", "w") as file:
                    file.write(str_score)

        # Jerry can spawn & collection logic
        if player_center.distance_to(time_pos) < 40:    
            jerry_can_sound.play()
            time_left += 8.0
            time_visible = False
            jerry_respawn_timer = random.uniform(5.0, 10.0) 

        if not time_visible:
            jerry_respawn_timer -= dt
            time_pos.x = 900
            time_pos.y = 900
            if jerry_respawn_timer <= 0:
                time_pos.x = random.randint(40, 760)
                time_pos.y = random.randint(40, 560)
                time_visible = True

    # --- DRAW GAMEPLAY (RUNS IN BOTH PLAYING AND PAUSED STATES) ---
    screen.fill("purple")

    screen.blit(coin_img, (coin_pos.x - 20, coin_pos.y - 20))
    if ruby_time_visible:
        screen.blit(bonus_img, (bonus_pos.x - 20, bonus_pos.y - 20))
    if time_visible:
        screen.blit(time_img, (time_pos.x - 20, time_pos.y - 20))

    screen.blit(playerImage, player_rect.topleft)

    score_surface = game_font.render(f"Score: {score}", True, "white")
    highscore_surface = game_font.render(f"High Score: {high_score}", True, "yellow")
    timer_surface = game_font.render(f"Time: {int(time_left)}s", True, "white")

    screen.blit(score_surface, (20, 20))       
    screen.blit(highscore_surface, (20, 60))  
    screen.blit(timer_surface, (650, 20))     

    # --- PAUSE OVERLAY ---
    if game_state == "PAUSED":
        overlay = pygame.Surface((800, 600), pygame.SRCALPHA)
        overlay.fill((0, 0, 0, 150))
        screen.blit(overlay, (0, 0))

        pause_txt = title_font.render("PAUSED", True, "white")
        resume_txt = game_font.render("Press P or ESC to Resume", True, "gray")

        screen.blit(pause_txt, pause_txt.get_rect(center=(400, 250)))
        screen.blit(resume_txt, resume_txt.get_rect(center=(400, 320)))

    pygame.display.flip()  
    dt = clock.tick(60) / 1000

pygame.quit()