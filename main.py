import pygame
import random
import os

# pygame setup
pygame.init()
screen = pygame.display.set_mode((800, 600))
clock = pygame.time.Clock()
running = True
dt = 0
game_over_sound_played = False
# Game variables
score = 0
speed = 300 

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

# Background Music (Loops continuously)
# Use .mp3 or .ogg for long music tracks
pygame.mixer.music.load("assets/music1.mp3") 
pygame.mixer.music.set_volume(0.4) # Sets volume between 0.0 and 1.0
pygame.mixer.music.play(-1)        # -1 tells pygame to loop it forever

# Sound Effects (Plays once instantly)
# Use .wav for short sound effects to avoid delay
jerry_can_sound = pygame.mixer.Sound("assets/ding.wav")
jerry_can_sound.set_volume(0.6)

coin_sound = pygame.mixer.Sound("assets/coin.wav")
coin_sound.set_volume(0.6)

bonus_sound = pygame.mixer.Sound("assets/bonus coin sound.wav")
bonus_sound.set_volume(0.6)

game_over_sound = pygame.mixer.Sound("assets/game over.wav")
game_over_sound.set_volume(0.6)

# --- FONT & TIMER SETUP ---
# Initialize the font module and create a font style
pygame.font.init()
game_font = pygame.font.SysFont("Arial", 30, bold=True)

# Set your starting time (e.g., 30 seconds)
time_left = 30.0 

# --- INITIAL HIGH SCORE LOAD ---
if os.path.exists("highscore.txt"):
    with open("highscore.txt", "r") as file:
        content = file.read().strip()
        high_score = int(content) if content.isdigit() else 0
else:
    high_score = 0

# Game loop state
game_over = False

# --- JERRY CAN SPAWN TIMER ---
time_visible = False         # Controls if the jerry can is drawn and collectable
jerry_respawn_timer = 0.0   # Timer tracking when it should reappear

# --- RUBY SPAWN TIMER ---
ruby_time_visible = False
ruby_spawn_timer = random.uniform(3.0, 6.0)

while running:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False
    # Countdown timer using delta time
    if not game_over:
        time_left -= dt
        if time_left <= 0:
            time_left = 0
            game_over = True

    if game_over:
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
        
        # Check for restart key input
        keys = pygame.key.get_pressed()
        if keys[pygame.K_r]:
            score = 0
            time_left = 30.0
            pos_x, pos_y = screen.get_width() / 2, screen.get_height() / 2
            game_over = False
            game_over_sound.stop()
            pygame.mixer.music.play(-1)
            
        pygame.display.flip()
        dt = clock.tick(60) / 1000
        continue # Skip the rest of the game logic this frame



    screen.fill("purple")

    # Draw collectible coins
    screen.blit(coin_img, (coin_pos.x - 20, coin_pos.y - 20))
    screen.blit(bonus_img, (bonus_pos.x - 20, bonus_pos.y - 20))
    if time_visible:
        screen.blit(time_img, (time_pos.x - 20, time_pos.y - 20))

    # CLEANED: The red pygame.draw.rect line has been removed completely.
    # The rect is now used invisibly behind the scenes just to place the image.
    screen.blit(playerImage, player_rect.topleft)

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

    # Update the invisible Rect object's center coordinates
    player_rect.centerx = int(pos_x)
    player_rect.centery = int(pos_y)

    # Clean screen boundary checks using built-in pygame.Rect properties
    if player_rect.right > 760:
        pos_x -= speed * dt
    if player_rect.left < 40:
        pos_x += speed * dt
    if player_rect.bottom > 560:
        pos_y -= speed * dt
    if player_rect.top < 40:
        pos_y += speed * dt
        
    # Re-apply boundaries to the object
    player_rect.centerx = int(pos_x)
    player_rect.centery = int(pos_y)

    # Check distance/collision using Rect center profiles
    player_center = pygame.Vector2(player_rect.center)
    
    if player_center.distance_to(coin_pos) < 40:    
        coin_pos.x = random.randint(40, 760)
        coin_pos.y = random.randint(40, 560)
        coin_sound.play()

        score += 1
        str_score = str(score) 
        print(score)
        
        if os.path.exists("highscore.txt"):
            with open("highscore.txt", "r") as file:
                high_score = int(file.read())
        else:
            high_score = 0
            with open("highscore.txt", "w") as file:
                file.write(str_score)

        if score >= high_score: 
            high_score = score
            with open("highscore.txt", "w") as file:
                file.write(str_score)
        print(high_score)
    if not ruby_time_visible:
        ruby_spawn_timer -= dt
        bonus_pos.x = 900
        bonus_pos.y = 900
        if ruby_spawn_timer <= 0:
            bonus_pos.x = random.randint(40, 760)
            bonus_pos.y = random.randint(40, 560)
            ruby_time_visible = True

    if player_center.distance_to(bonus_pos) < 40:    
        bonus_pos.x = random.randint(40, 760)
        bonus_pos.y = random.randint(40, 560)
        bonus_sound.play()

        ruby_spawn_timer = random.uniform(3.0, 6.0)

        ruby_time_visible = False

        score += 5
        str_score = str(score) 
        print(score)

        if os.path.exists("highscore.txt"):
            with open("highscore.txt", "r") as file:
                high_score = int(file.read())
        else:
            high_score = 0
            with open("highscore.txt", "w") as file:
                file.write(str_score)

        if score >= high_score: 
            high_score = score
            with open("highscore.txt", "w") as file:
                file.write(str_score)

    if player_center.distance_to(time_pos) < 40:    
        jerry_can_sound.play()
        time_left += 8.0

        # Hide the jerry can instantly
        time_visible = False
        
        # Set a random delay (in seconds) before it spawns again
        # This forces the player to survive on limited fuel!
        jerry_respawn_timer = random.uniform(5.0, 10.0) 
    # If the jerry can was collected, count down until it respawns
    if not time_visible:
        jerry_respawn_timer -= dt
        time_pos.x = 900
        time_pos.y = 900
        if jerry_respawn_timer <= 0:
            # Re-locate and reveal the jerry can after the wait time ends
            time_pos.x = random.randint(40, 760)
            time_pos.y = random.randint(40, 560)
            time_visible = True



    # 1. Render the text strings into images
    # (Text, Antialiasing, Color)
    score_surface = game_font.render(f"Score: {score}", True, "white")
    highscore_surface = game_font.render(f"High Score: {high_score}", True, "yellow")
    timer_surface = game_font.render(f"Time: {int(time_left)}s", True, "white")

    # 2. Draw them onto the screen at specific (X, Y) coordinates
    screen.blit(score_surface, (20, 20))       # Top Left
    screen.blit(highscore_surface, (20, 60))  # Under the score
    screen.blit(timer_surface, (650, 20))     # Top Right


    pygame.display.flip() 
    dt = clock.tick(60) / 1000

pygame.quit()
