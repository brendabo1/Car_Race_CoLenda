#include "mouse_module.h"
#include "colision_module.h"
#include "../Lib/colenda.h"
#include "../drivers/pushbuttons/keys.c"
#include "../drivers/7seg_display/display_7seg.c"
#include "background_animation_module.h"
#include "create_cover.c"
#include "create_letters.c"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define PLAYER_SPEED_BASE 2
#define OFFSET_PLAYER_1 17
#define OFFSET_PLAYER_2 18
#define BULLET_SPEED_BASE 15

/*criação de um tipo pra facilitar a manipulação dos estados do jogo*/
typedef enum {
    running = 1,
    in_pause,
    in_menu,
    finish,
    win,
    lose,
} states;

/*criação de um tipo para facilitar a manipulação dos modos de jogo*/
typedef enum {
    single_player = 1,
    dual_player,
} game_modes;

//do registrador 1 ao 10 são as balas, do 11 ao 19 são cenarios, do 20 ao 29 são obstaculos, 30 player 2 e 31 player 1
obstacle_t obs[13];
obstacle_t screen_obs[10];
sprite_t cenario[9];
sprite_t sprite_bullets[10];

sprite_t player_1_sprite = {
    .data_register = 31,
    .coord_x = 192,
    .coord_y = 340,
    .offset = OFFSET_PLAYER_1,
    .speed = PLAYER_SPEED_BASE,
    .visibility = 1,
};
sprite_t player_2_sprite = {
    .data_register = 30,
    .coord_x = 432,
    .coord_y = 340,
    .offset = OFFSET_PLAYER_2,
    .speed = PLAYER_SPEED_BASE,
    .visibility = 1,
};

sprite_t invisible_sprite = {
    .coord_x = 1, 
    .coord_y = 1, 
    .offset = 0, 
    .speed = 0, 
    .visibility = 0};
//mutex utilizados
pthread_mutex_t gpu_mutex;
pthread_mutex_t background_mutex;
pthread_mutex_t mouse_1_mutex;
pthread_mutex_t mouse_2_mutex;
pthread_mutex_t obstacle_mutex;
pthread_mutex_t bullets_mutex;
pthread_mutex_t player_1_invunerability_mutex;
pthread_mutex_t player_2_invunerability_mutex;
pthread_mutex_t colision_mutex;
//condicionais utilizadas
pthread_cond_t mouse_1_cond;
pthread_cond_t mouse_2_cond;
pthread_cond_t obstacle_cond;
pthread_cond_t background_cond;
pthread_cond_t bullets_cond;
pthread_cond_t player_1_invulnerability_cond;
pthread_cond_t player_2_invulnerability_cond;
pthread_cond_t colision_cond;
//threads
pthread_t obstacle_thread, background_thread, mouse_1_thread, mouse_2_thread, player_1_timer_thread, player_2_timer_thread, bullets_thread, colision_thread; 

states state;
game_modes current_game_mode;

int pause_background, pause_obstacle, pause_mouse_1, pause_mouse_2, pause_bullets, pause_colision;
int player_1_invunerability;
int player_2_invunerability;
int obstacle_on_screen_status[10] = {0,0,0,0,0,0,0,0,0,0};
int score_1;
int score_2;
int bullets[10] = {0,0,0,0,0,0,0,0,0,0};
int key_press_1;
int key_press_2;

void pause_threads() {
    
    pthread_mutex_lock(&mouse_2_mutex);
    pause_mouse_2 = 1;
    pthread_mutex_unlock(&mouse_2_mutex);
    pthread_mutex_lock(&mouse_1_mutex);
    pause_mouse_1 = 1;
    pthread_mutex_unlock(&mouse_1_mutex);
    pthread_mutex_lock(&bullets_mutex);
    pause_bullets = 1;
    pthread_mutex_unlock(&bullets_mutex);
    pthread_mutex_lock(&obstacle_mutex);
    pause_obstacle = 1;
    pthread_mutex_unlock(&obstacle_mutex);
    pthread_mutex_lock(&background_mutex);
    pause_background = 1;
    pthread_mutex_unlock(&background_mutex);
    pthread_mutex_lock(&colision_mutex);
    pause_colision = 1;
    pthread_mutex_unlock(&colision_mutex);
}

void reestart_threads() {
    
    if (current_game_mode == dual_player) {
        pthread_mutex_lock(&mouse_2_mutex);
        pause_mouse_2 = 0;
        pthread_cond_signal(&mouse_2_cond);
        pthread_mutex_unlock(&mouse_2_mutex);
    }
    pthread_mutex_lock(&mouse_1_mutex);
    pause_mouse_1 = 0;
    pthread_cond_signal(&mouse_1_cond);
    pthread_mutex_unlock(&mouse_1_mutex);
    pthread_mutex_lock(&bullets_mutex);
    pause_bullets = 0;
    pthread_cond_signal(&bullets_cond);
    pthread_mutex_unlock(&bullets_mutex);
    pthread_mutex_lock(&obstacle_mutex);
    pause_obstacle = 0;
    pthread_cond_signal(&obstacle_cond);
    pthread_mutex_unlock(&obstacle_mutex);
    pthread_mutex_lock(&background_mutex);
    pause_background = 0;
    pthread_cond_signal(&background_cond);
    pthread_mutex_unlock(&background_mutex);
    pthread_mutex_lock(&colision_mutex);
    pause_colision = 0;
    pthread_cond_signal(&colision_cond);
    pthread_mutex_unlock(&colision_mutex);
}

void* player_1_invulnerability_timer(void* args) {
    int i = 0;
    while(state != finish){
        pthread_mutex_lock(&player_1_invunerability_mutex);
        while(!player_1_invunerability || state == in_pause || state == in_menu || state == win || state == lose){
            pthread_cond_wait(&player_1_invulnerability_cond, &player_1_invunerability_mutex);
        }
        pthread_mutex_unlock(&player_1_invunerability_mutex);

        pthread_mutex_lock(&mouse_1_mutex);
        pause_mouse_1 = 1;
        pthread_mutex_unlock(&mouse_1_mutex);
        
        if(player_1_sprite.visibility) {
            player_1_sprite.visibility = 0;
        } else {
            player_1_sprite.visibility = 1;
        }
        pthread_mutex_lock(&gpu_mutex);
        set_sprite(player_1_sprite);
        pthread_mutex_unlock(&gpu_mutex);
        pthread_mutex_lock(&mouse_1_mutex);
        pause_mouse_1 = 0;
        pthread_cond_signal(&mouse_1_cond);
        pthread_mutex_unlock(&mouse_1_mutex);
        
        if (i == 10) {
            pthread_mutex_lock(&player_1_invunerability_mutex);
            player_1_invunerability = 0;
            pthread_mutex_unlock(&player_1_invunerability_mutex);
            i=0;
            player_1_sprite.visibility = 1;

            pthread_mutex_lock(&gpu_mutex);
            set_sprite(player_1_sprite);
            pthread_mutex_unlock(&gpu_mutex);
        }else {
            i++;
        }
        usleep(200000);
    }
}

void* player_2_invulnerability_timer(void* args) {
    int i = 0;
    while(state != finish){
        pthread_mutex_lock(&player_2_invunerability_mutex);
        while(!player_2_invunerability || state == in_pause || state == in_menu || state == win || state == lose){
            pthread_cond_wait(&player_2_invulnerability_cond, &player_2_invunerability_mutex);
        }
        pthread_mutex_unlock(&player_2_invunerability_mutex);
        pthread_mutex_lock(&mouse_2_mutex);
        pause_mouse_2 = 1;
        pthread_mutex_unlock(&mouse_2_mutex);
        if (player_2_sprite.visibility) {
            player_2_sprite.visibility = 0;
        }
        else {
            player_2_sprite.visibility = 1;
        }

        pthread_mutex_lock(&gpu_mutex);
        set_sprite(player_2_sprite);
        pthread_mutex_unlock(&gpu_mutex);

        pthread_mutex_lock(&mouse_2_mutex);
        pause_mouse_2 = 0;
        pthread_cond_signal(&mouse_2_cond);
        pthread_mutex_unlock(&mouse_2_mutex);

        if(i == 10) {
            pthread_mutex_lock(&player_2_invunerability_mutex);
            player_2_invunerability = 0;
            pthread_mutex_unlock(&player_2_invunerability_mutex);
            i = 0;
            player_2_sprite.visibility = 1;
            pthread_mutex_lock(&gpu_mutex);
            set_sprite(player_2_sprite);
            pthread_mutex_unlock(&gpu_mutex);
        } else {
            i++;
        }
        usleep(200000);
    }
}

void* bullet_routine(void* args) {
    while (state != finish) {
        pthread_mutex_lock(&bullets_mutex);
        while (pause_bullets || state == in_pause || state == in_menu || state == win || state == lose)
        {
            pthread_cond_wait(&bullets_cond, &bullets_mutex);
        }
        pthread_mutex_unlock(&bullets_mutex);
        
        for (int i = 0; i < 10; i++)
        {
            if (i < 5) {
                if(bullets[i]) {
                    pthread_mutex_lock(&mouse_1_mutex);
                    pause_mouse_1 = 1;
                    pthread_mutex_unlock(&mouse_1_mutex);
                    
                    if (sprite_bullets[i].coord_y <= 15) {
                        bullets[i] = 0;
                        sprite_bullets[i].visibility = 0;
                    } else {
                        sprite_bullets[i].coord_y -= BULLET_SPEED_BASE;
                    }
                    pthread_mutex_lock(&mouse_1_mutex);
                    pause_mouse_1 = 0;
                    pthread_cond_signal(&mouse_1_cond);
                    pthread_mutex_unlock(&mouse_1_mutex);
                    pthread_mutex_lock(&gpu_mutex);
                    set_sprite(sprite_bullets[i]);
                    pthread_mutex_unlock(&gpu_mutex);
                }
            } else {
                if(bullets[i]) {
                    pthread_mutex_lock(&mouse_2_mutex);
                    pause_mouse_2 = 1;
                    pthread_mutex_unlock(&mouse_2_mutex);
                    
                    if (sprite_bullets[i].coord_y <= 15) {
                        bullets[i] = 0;
                        sprite_bullets[i].visibility = 0;
                    } else {
                        sprite_bullets[i].coord_y -= BULLET_SPEED_BASE;
                    }
                    pthread_mutex_lock(&mouse_2_mutex);
                    pause_mouse_2 = 0;
                    pthread_cond_signal(&mouse_2_cond);
                    pthread_mutex_unlock(&mouse_2_mutex);
                    pthread_mutex_lock(&gpu_mutex);
                    set_sprite(sprite_bullets[i]);
                    pthread_mutex_unlock(&gpu_mutex);
                }
            }
        }
        
        usleep(100000);
    }
}

//esta rotina já esta completa, apenas basta ver o tempo de delay de alteração do fundo 
void* change_background_routine(void* args) {

    while (state != finish) {
        pthread_mutex_lock(&background_mutex);
        while (pause_background || state == in_pause || state == in_menu || state == win || state == lose)
        {
            pthread_cond_wait(&background_cond, &background_mutex);
        }
        pthread_mutex_unlock(&background_mutex);

        pthread_mutex_lock(&gpu_mutex);
        bg_animation(); //função do modulo que faz a atualização do fundo e da animação com o passar dos quadros
        pthread_mutex_unlock(&gpu_mutex);
        usleep(100000);
    }
}

void* mouse_1_polling_routine(void* args) {
    
    int value_x_mouse = 0, i, has_shot = 0;
    int car_speed;

    while (state != finish) {
        pthread_mutex_lock(&mouse_1_mutex);
        while (pause_mouse_1 || state == in_pause || state == in_menu || state == win || state == lose)
        {
            pthread_cond_wait(&mouse_1_cond, &mouse_1_mutex);
        }
        pthread_mutex_unlock(&mouse_1_mutex);
        read_mouse_1_event(&key_press_1, &value_x_mouse);

        car_speed = PLAYER_SPEED_BASE * value_x_mouse;
        //detecção de borda
        if ( car_speed < 0 && player_1_sprite.coord_x - 10 >= 96 && (player_1_sprite.coord_x - 10) + car_speed <= 96) {
            //pega o espaço restante que o carro ainda pode se mover antes de chegar na borda
            player_1_sprite.coord_x -= ((player_1_sprite.coord_x - 10 + car_speed) - 96); 
        } else if (car_speed > 0 && player_1_sprite.coord_x + 10 <= 289 && (player_1_sprite.coord_x + 10) + car_speed >= 289) {
            //pega o espaço restante que o carro ainda pode se mover antes de chegar na borda
            player_1_sprite.coord_x += (289 - (player_1_sprite.coord_x + 10 + car_speed));
        } else { 
            player_1_sprite.coord_x += car_speed;
        }

        if (key_press_1 == 1 && !has_shot) {
            i = 0;
            pthread_mutex_lock(&bullets_mutex);
            pause_bullets = 1;
            pthread_mutex_unlock(&bullets_mutex);
            
            while(i < 5){
                if (bullets[i] == 0) {
                    bullets[i] = 1;
                    sprite_bullets[i].offset = LASER_VERTICAL;
                    sprite_bullets[i].coord_x = player_1_sprite.coord_x;
                    sprite_bullets[i].coord_y = player_1_sprite.coord_y - 20;
                    sprite_bullets[i].visibility = 1;
                    sprite_bullets[i].data_register = (i + 1);
                    sprite_bullets[i].speed = BULLET_SPEED_BASE;
                    has_shot = 1;
                    break;
                }
                i++;
            }
            
            pthread_mutex_lock(&bullets_mutex);
            pause_bullets = 0;
            pthread_cond_signal(&bullets_cond);
            pthread_mutex_unlock(&bullets_mutex);
        } else if (key_press_1){
            has_shot = 1;
        } else {
            has_shot = 0;
        }

        pthread_mutex_lock(&gpu_mutex);
        set_sprite(player_1_sprite);
        if(i < 5 ) {
            set_sprite(sprite_bullets[i]);
        }
        pthread_mutex_unlock(&gpu_mutex);
    }
}

void* mouse_2_polling_routine(void* args) {
    
    int value_x_mouse = 0, i, has_shot = 0;
    int car_speed;

    while (state != finish) {
        pthread_mutex_lock(&mouse_2_mutex);
        while (pause_mouse_2 || state == in_pause || state == in_menu || state == win || state == lose || current_game_mode == single_player)
        {
            pthread_cond_wait(&mouse_2_cond, &mouse_2_mutex);
        }
        pthread_mutex_unlock(&mouse_2_mutex);
        read_mouse_2_event(&key_press_2, &value_x_mouse);
        car_speed = PLAYER_SPEED_BASE * value_x_mouse;

        //detecção de borda
        if ( car_speed < 0 && player_2_sprite.coord_x - 10 >= 336 && (player_2_sprite.coord_x - 10) + car_speed <= 336) {
            //pega o espaço restante que o carro ainda pode se mover antes de chegar na borda
            player_2_sprite.coord_x -= ((player_2_sprite.coord_x - 10 + car_speed) - 336); 
        } else if (car_speed > 0 && player_2_sprite.coord_x + 10 <= 528 && (player_2_sprite.coord_x + 10) + car_speed >= 528) {
            //pega o espaço restante que o carro ainda pode se mover antes de chegar na borda
            player_2_sprite.coord_x += (528 - (player_2_sprite.coord_x + 10 + car_speed));
        } else { 
            player_2_sprite.coord_x += car_speed;
        }

        if (key_press_2 == 1 && !has_shot) {
            i = 5;
            pthread_mutex_lock(&bullets_mutex);
            pause_bullets = 1;
            pthread_mutex_unlock(&bullets_mutex);
            while(i < 10){
        
                if (bullets[i] == 0) {
                    bullets[i] = 1;
                    sprite_bullets[i].offset = LASER_VERTICAL;
                    sprite_bullets[i].coord_x = player_2_sprite.coord_x;
                    sprite_bullets[i].coord_y = player_2_sprite.coord_y - 20;
                    sprite_bullets[i].visibility = 1;
                    sprite_bullets[i].data_register = (i + 1);
                    sprite_bullets[i].speed = BULLET_SPEED_BASE;
                    has_shot = 1;
                    break;
                }
                
                i++;
            }
            pthread_mutex_lock(&bullets_mutex);
            pause_bullets = 0;
            pthread_cond_signal(&bullets_cond);
            pthread_mutex_unlock(&bullets_mutex);
        } else if (key_press_2){
            has_shot = 1;
        } else {
            has_shot = 0;
        }

        pthread_mutex_lock(&gpu_mutex);
        set_sprite(player_2_sprite);
        if(i<10) {
            set_sprite(sprite_bullets[i]);
        }
        pthread_mutex_unlock(&gpu_mutex);
        
    }
}

void* random_obstacle_generate_routine(void* args) {
    while (state != finish)
    {
        pthread_mutex_lock(&obstacle_mutex);
        while (pause_obstacle || state == in_pause || state == in_menu || state == win || state == lose)
        {
            pthread_cond_wait(&obstacle_cond, &obstacle_mutex);
        }
        pthread_mutex_unlock(&obstacle_mutex);

        pthread_mutex_lock(&gpu_mutex);
        if(current_game_mode == dual_player) {
            random_obstacle(player_2_sprite.coord_x, player_2_sprite.coord_y, 336, 528, screen_obs, obs, obstacle_on_screen_status);
        }
        random_obstacle(player_1_sprite.coord_x, player_1_sprite.coord_y, 96, 289, screen_obs, obs, obstacle_on_screen_status);
        pthread_mutex_unlock(&gpu_mutex);
        usleep(100000);
    }
}

// loop principal do jogo
void* colision_routine(void* args){

    int points = 0;
    
    while (state != finish)
    {
        pthread_mutex_lock(&colision_mutex);
        while(pause_colision || state == in_pause || state == in_menu || state == win || state == lose){
            pthread_cond_wait(&colision_cond, &colision_mutex);
        }
        pthread_mutex_unlock(&colision_mutex);
        for (int i = 0; i < 10; i++)
        {
            if(obstacle_on_screen_status[i]) {
                pthread_mutex_lock(&obstacle_mutex);
                pause_obstacle = 1;
                pthread_mutex_unlock(&obstacle_mutex);
                for (int j = 0; j < 10; j++)
                {
                    if (bullets[j]) {
                        pthread_mutex_lock(&bullets_mutex);
                        pause_bullets = 1;
                        pthread_mutex_unlock(&bullets_mutex);
                        if (check_colision_bullet(sprite_bullets[j], screen_obs[i])) {
                            points = screen_obs[i].reward;
                            sprite_bullets[j].visibility = 0;
                            bullets[j] = 0;
                            obstacle_on_screen_status[i] = 0;
                            screen_obs[i].on_frame = 0;
                            invisible_sprite.visibility = i + 20;
                            pthread_mutex_lock(&gpu_mutex);
                            set_sprite(invisible_sprite);
                            set_sprite(sprite_bullets[j]);
                            pthread_mutex_unlock(&gpu_mutex);
                            
                        } else {
                            points = 0;
                        }
                        pthread_mutex_lock(&bullets_mutex);
                        pause_bullets = 0;
                        pthread_cond_signal(&bullets_cond);
                        pthread_mutex_unlock(&bullets_mutex);
                        if ( (j < 5 && current_game_mode == dual_player) || (j < 10 && current_game_mode == single_player)){
                            score_1 += points;
                            display_write_score(score_1, 1);
                        } else {
                            score_2 += points;
                            display_write_score(score_2, 0);
                        }
                    }
                }
                pthread_mutex_lock(&obstacle_mutex);
                pause_obstacle = 0;
                pthread_cond_signal(&obstacle_cond);
                pthread_mutex_unlock(&obstacle_mutex);
            }
        }

        pthread_mutex_lock(&player_1_invunerability_mutex);
        if (!player_1_invunerability){
            for (int i = 0; i < 10; i++) {
                if(obstacle_on_screen_status[i]) {
                    pthread_mutex_lock(&obstacle_mutex);
                    pause_obstacle = 1;
                    pthread_mutex_unlock(&obstacle_mutex);

                    pthread_mutex_lock(&mouse_1_mutex);
                    pause_mouse_1 = 1;
                    pthread_mutex_unlock(&mouse_1_mutex);

                    if(check_colision_player(player_1_sprite, screen_obs[i])){
                        score_1 -= screen_obs[i].reward;
                        player_1_invunerability = 1;
                        obstacle_on_screen_status[i] = 0;
                        screen_obs[i].on_frame = 0;
                        invisible_sprite.data_register = 20 + i;
                        pthread_mutex_lock(&gpu_mutex);
                        set_sprite(invisible_sprite);
                        pthread_mutex_unlock(&gpu_mutex);
                        pthread_cond_signal(&player_1_invulnerability_cond);
                        
                    }
                    pthread_mutex_lock(&mouse_1_mutex);
                    pause_mouse_1 = 0;
                    pthread_cond_signal(&mouse_1_cond);
                    pthread_mutex_unlock(&mouse_1_mutex);
                    pthread_mutex_lock(&obstacle_mutex);
                    pause_obstacle = 0;
                    pthread_cond_signal(&obstacle_cond);
                    pthread_mutex_unlock(&obstacle_mutex);
                    if(player_1_invunerability) {
                        break;
                    }
                }
            }
        }
        pthread_mutex_unlock(&player_1_invunerability_mutex);
        
        pthread_mutex_lock(&player_2_invunerability_mutex);
        if (!player_2_invunerability && current_game_mode == dual_player){
            for (int i = 0; i < 10; i++) {
                if(obstacle_on_screen_status[i]) {
                    pthread_mutex_lock(&obstacle_mutex);
                    pause_obstacle = 1;
                    pthread_mutex_unlock(&obstacle_mutex);
                    pthread_mutex_lock(&mouse_2_mutex);
                    pause_mouse_2 = 1;
                    pthread_mutex_unlock(&mouse_2_mutex);

                    if(check_colision_player(player_2_sprite, screen_obs[i])) {
                        score_2 -= screen_obs[i].reward;
                        player_2_invunerability = 1;
                        obstacle_on_screen_status[i] = 0;
                        screen_obs[i].on_frame = 0;
                        invisible_sprite.data_register = 20 + i;
                        pthread_mutex_lock(&gpu_mutex);
                        set_sprite(invisible_sprite);
                        pthread_mutex_unlock(&gpu_mutex);
                        pthread_cond_signal(&player_2_invulnerability_cond);
                    }
                    pthread_mutex_lock(&mouse_2_mutex);
                    pause_mouse_2 = 0;
                    pthread_cond_signal(&mouse_2_cond);
                    pthread_mutex_unlock(&mouse_2_mutex);
                    pthread_mutex_lock(&obstacle_mutex);
                    pause_obstacle = 0;
                    pthread_cond_signal(&obstacle_cond);
                    pthread_mutex_unlock(&obstacle_mutex);
                    if (player_2_invunerability) {
                        break;
                    }
                }
            }
        }
        pthread_mutex_unlock(&player_2_invunerability_mutex);

        if (score_1 >= 0) {
            display_write_score(score_1, 1);
        
        }
        if (score_2 >= 0 && current_game_mode == dual_player) {
            display_write_score(score_2, 0);
        }
        
        if(score_1 < 0) {
            printf("jogador 1 perdeu\n");
            state = lose;
            lose_screen(); // colocar o jogar que perdeu
        } else if (score_2 < 0 && current_game_mode == dual_player) {
            printf("jogador 2 perdeu\n");
            state = lose;
            lose_screen(); // colocar o jogador que perdeu
        } else if (score_1 >= 1000 && score_2 >= 1000) {
            printf("empate\n");
            state = win;
            win_screen(); //mudar pra tela de empate
        } else if (score_1 >= 1000) {
            printf("jogador 1 venceu\n");
            state = win;
            win_screen(); //adicionar a opção do jogador que venceu
        } else if (score_2 >= 1000) {
            printf("jogador 2 venceu\n");
            state = win;
            win_screen(); //adicionar a opção do jogador que venceu
        }
    }
}

void pause_screen() {
    int msg_letters[5] = {P, A, U, S, E};
    pause_threads();
    module_exit_mouse_1();
    module_exit_mouse_2();

    pthread_mutex_lock(&gpu_mutex);
    for (int i = 0; i < 10; i++)
    {
        
        if(obstacle_on_screen_status[i]){
            invisible_sprite.data_register = 20 + i;
            set_sprite(invisible_sprite);
        }
        if (bullets[i]) {
            invisible_sprite.data_register = 1 + i;
            set_sprite(invisible_sprite);
        }
        
    }
    player_1_sprite.visibility = 0;
    player_2_sprite.visibility = 0;
    set_sprite(player_1_sprite);
    set_sprite(player_2_sprite);

    for (int i = 0; i < 5; i++)
    {
        cenario[i].coord_x = 280 + (20 * i);
        cenario[i].coord_y = 240;
        cenario[i].data_register = 11 + i;
        cenario[i].visibility = 1;
        cenario[i].offset = msg_letters[i];
        cenario[i].speed = 0;
        set_sprite(cenario[i]);
    }
    
    pthread_mutex_unlock(&gpu_mutex);

}

void return_screen() {
    module_init_mouse_1();
    module_init_mouse_2();
    pthread_mutex_lock(&gpu_mutex);
    player_1_sprite.visibility = 1;
    if(current_game_mode == dual_player) {
        player_2_sprite.visibility = 1;
    }

    for (int i = 0; i < 5; i++) {
        cenario[i].visibility = 0;
        set_sprite(cenario[i]);
    }
    set_sprite(player_1_sprite);
    set_sprite(player_2_sprite);
    pthread_mutex_unlock(&gpu_mutex);
    reestart_threads();
    state = running;
}

void win_screen() {
    int coord_x = 0;
    int msg_letters[6] = {V, E, N, C, E, U};
    pause_threads();
    
    for (int i = 0; i < 10; i++)
    {
        if (obstacle_on_screen_status[i]) {
            invisible_sprite.data_register = 20 + i;
            pthread_mutex_lock(&gpu_mutex);
            set_sprite(invisible_sprite);
            pthread_mutex_unlock(&gpu_mutex);
        } 
        if (bullets[i]) {
            invisible_sprite.data_register = 1 + i;
            pthread_mutex_lock(&gpu_mutex);
            set_sprite(invisible_sprite);
            pthread_mutex_unlock(&gpu_mutex);
        }
    }
    
    
    if (score_1 >= 1000) {
        for(int i = player_1_sprite.coord_y; i >=0; --i) {
            player_1_sprite.coord_y = i;
            pthread_mutex_lock(&gpu_mutex);
            set_sprite(player_1_sprite);
            pthread_mutex_unlock(&gpu_mutex);
            coord_x = 130;
        }
    } else {
        for(int i = player_2_sprite.coord_y; i >=0; --i) {
            player_2_sprite.coord_y = i;
            pthread_mutex_lock(&gpu_mutex);
            set_sprite(player_2_sprite);
            pthread_mutex_unlock(&gpu_mutex);
            coord_x = 370;
        }
    }

    for(int i = 0; i < 6; i++) {
        cenario[i].coord_x = coord_x + (20 * i);
        cenario[i].coord_y = 240;
        cenario[i].data_register = 11 + i;
        cenario[i].visibility = 1;
        cenario[i].offset = msg_letters[i];
        cenario[i].speed = 0;
        set_sprite(cenario[i]);
    }

    sleep(1);

    state = in_menu;
    module_exit_mouse_1();
    module_exit_mouse_2();
    clear();
    draw_cover_art();
}


void lose_screen() {

    int coord_x;
    int msg_letters[6] = {P, E, R, D, E, U};
    pause_threads();
    while(player_1_invunerability || player_2_invunerability){
    }
    if(score_1 < 0) {
        coord_x = 130;
    } else {
        coord_x = 370;
    }
    for (int i = 0; i < 10; i++)
    {
        if (obstacle_on_screen_status[i]) {
            invisible_sprite.data_register = 20 + i;
            pthread_mutex_lock(&gpu_mutex);
            set_sprite(invisible_sprite);
            pthread_mutex_unlock(&gpu_mutex);
        } 
        if (bullets[i]) {
            invisible_sprite.data_register = 1 + i;
            pthread_mutex_lock(&gpu_mutex);
            set_sprite(invisible_sprite);
            pthread_mutex_unlock(&gpu_mutex);
        }
    }

    player_1_sprite.visibility = 0;
    player_2_sprite.visibility = 0;

    set_sprite(player_1_sprite);
    set_sprite(player_2_sprite);
    
    player_1_invunerability = 0;
    player_2_invunerability = 0;

    for (int i = 0; i < 6; i++) {
        cenario[i].coord_x = coord_x + (20 * i);
        cenario[i].coord_y = 240;
        cenario[i].data_register = 11 + i;
        cenario[i].visibility = 1;
        cenario[i].offset = msg_letters[i];
        cenario[i].speed = 0;
        set_sprite(cenario[i]);
    }

    sleep(1);


    clear();
    state = in_menu;
    draw_cover_art();
    module_exit_mouse_1();
    module_exit_mouse_2();
    
}   

void init_game() {

    clear();
    score_1 = 0;
    score_2 = 0;
    for(int i = 0; i < 10; i++){
        if (bullets[i]) {
            sprite_bullets[i].visibility = 0;
            bullets[i] = 0;
            pthread_mutex_lock(&gpu_mutex);
            set_sprite(sprite_bullets[i]);
            pthread_mutex_unlock(&gpu_mutex);
        }
        if (obstacle_on_screen_status[i]) {
            screen_obs[i].on_frame = 0;
            obstacle_on_screen_status[i] = 0;
            invisible_sprite.data_register = 20 + i;
            pthread_mutex_lock(&gpu_mutex);
            set_sprite(invisible_sprite);
            pthread_mutex_unlock(&gpu_mutex);
        }
    }
    
    player_1_sprite.visibility = 1;

    if(current_game_mode == dual_player) {
        player_2_sprite.visibility = 1;
    }
    
    module_init_mouse_1();
    module_init_mouse_2();
    bg_animation_module_init();
    set_sprite(player_1_sprite);
    if(current_game_mode == dual_player) {
        set_sprite(player_2_sprite);
    }
    
    reestart_threads();
    
    
}

void menu() {
    state = in_menu;
    char btn_pressed;
    while(state != finish) {
        KEYS_read(&btn_pressed);
        // scanf("%c", &btn_pressed);

        if (btn_pressed == BUTTON0 && state == in_menu) {
            current_game_mode = single_player;
            state = running;
            init_game();
        } else if (btn_pressed == BUTTON1 && state == in_menu){
            current_game_mode = dual_player;
            state = running;
            init_game();
        } else if (btn_pressed == BUTTON1 && state == running) {
            state = in_pause;
            pause_screen();
        } else if (btn_pressed == BUTTON1 && state == in_pause) {
            state = running;
            return_screen();
        } else if (btn_pressed == BUTTON2) {
            state = in_menu;
            pause_threads();
            for(int i = 0; i < 10; i++){
                if (bullets[i]) {
                    sprite_bullets[i].visibility = 0;
                    bullets[i] = 0;
                    pthread_mutex_lock(&gpu_mutex);
                    set_sprite(sprite_bullets[i]);
                    pthread_mutex_unlock(&gpu_mutex);
                }
                if (obstacle_on_screen_status[i]) {
                    screen_obs[i].on_frame = 0;
                    obstacle_on_screen_status[i] = 0;
                    invisible_sprite.data_register = 20 + i;
                    set_sprite(invisible_sprite);
                }
            }
            player_1_sprite.visibility = 0;
            player_2_sprite.visibility = 0;
            score_1 = 0;
            score_2 = 0;
            set_sprite(player_1_sprite);
            set_sprite(player_2_sprite);
            draw_cover_art();
        } else if (btn_pressed == BUTTON3) {
            state = finish;
        } 

    }
    return;
}

int main() {

    //inicializacao dos inteiros
    score_1 = 0;
    score_2 = 0;
    pause_background = 1;
    pause_mouse_1 = 1;
    pause_mouse_2 = 1;
    pause_obstacle = 1;
    player_1_invunerability = 0;
    player_2_invunerability = 0;
    pause_bullets = 1;
    pause_colision = 1;
    key_press_1 = 0;
    key_press_2 = 0;
    state = in_menu;
    current_game_mode = dual_player;

    GPU_open();
    display_open();
    KEYS_open();
    charge_letters_on_memory();
    printf("finish charge\n");

    initialize_obstacle_vector(obs);
    clear();
    
    draw_cover_art();

    //inicialização dos mutex
    pthread_mutex_init(&gpu_mutex, NULL);
    pthread_mutex_init(&background_mutex, NULL);
    pthread_mutex_init(&mouse_1_mutex, NULL);
    pthread_mutex_init(&mouse_2_mutex, NULL);
    pthread_mutex_init(&obstacle_mutex, NULL);
    pthread_mutex_init(&player_1_invunerability_mutex, NULL);
    pthread_mutex_init(&player_2_invunerability_mutex, NULL);
    pthread_mutex_init(&bullets_mutex, NULL);
    pthread_mutex_init(&colision_mutex, NULL);

    //inicialização das condições
    pthread_cond_init(&background_cond, NULL);
    pthread_cond_init(&mouse_1_cond, NULL);
    pthread_cond_init(&mouse_2_cond, NULL);
    pthread_cond_init(&obstacle_cond, NULL);
    pthread_cond_init(&player_1_invulnerability_cond, NULL);
    pthread_cond_init(&player_2_invulnerability_cond, NULL);
    pthread_cond_init(&bullets_cond, NULL);
    pthread_cond_init(&colision_cond, NULL);

    //inicialização das threads
    
    pthread_create(&background_thread, NULL, change_background_routine, NULL);
    pthread_create(&mouse_1_thread, NULL, mouse_1_polling_routine, NULL);
    pthread_create(&mouse_2_thread, NULL, mouse_2_polling_routine, NULL);
    pthread_create(&obstacle_thread, NULL, random_obstacle_generate_routine, NULL);
    pthread_create(&bullets_thread, NULL, bullet_routine, NULL);
    pthread_create(&colision_thread, NULL, colision_routine, NULL);
    pthread_create(&player_1_timer_thread, NULL, player_1_invulnerability_timer, NULL);
    pthread_create(&player_2_timer_thread, NULL, player_2_invulnerability_timer, NULL);

    //loop principal do jogo
    menu();
    
    //TODO: tela de finalizar o jogo
    
    //finalizando as threads
    pthread_join(obstacle_thread, NULL);
    pthread_join(background_thread, NULL);
    pthread_join(mouse_1_thread, NULL);
    pthread_join(mouse_2_thread, NULL);
    pthread_join(player_1_timer_thread, NULL);
    pthread_join(player_2_timer_thread, NULL);
    pthread_join(bullets_thread, NULL);
    pthread_join(colision_thread, NULL);

    //encerrando os mutex
    pthread_mutex_destroy(&gpu_mutex);
    pthread_mutex_destroy(&background_mutex);
    pthread_mutex_destroy(&mouse_1_mutex);
    pthread_mutex_destroy(&mouse_2_mutex);
    pthread_mutex_destroy(&obstacle_mutex);
    pthread_mutex_destroy(&player_1_invunerability_mutex);
    pthread_mutex_destroy(&player_2_invunerability_mutex);
    pthread_mutex_destroy(&bullets_mutex);
    pthread_mutex_destroy(&colision_mutex);

    //encerrando as condicionais
    pthread_cond_destroy(&mouse_1_cond);
    pthread_cond_destroy(&mouse_2_cond);
    pthread_cond_destroy(&obstacle_cond);
    pthread_cond_destroy(&background_cond);
    pthread_cond_destroy(&player_1_invulnerability_cond);
    pthread_cond_destroy(&player_2_invulnerability_cond);
    pthread_cond_destroy(&bullets_cond);
    pthread_cond_destroy(&colision_cond);

    module_exit_mouse_1();
    module_exit_mouse_2();
    KEYS_close();
    display_close();
    GPU_close();
    return 0;
}