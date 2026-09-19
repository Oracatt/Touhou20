#pragma once
#include "option_frame.hpp"
#include "stage_reset.hpp"
namespace th20::source::player_entity {
class MovementServices {
public:
    virtual ~MovementServices()=default;
    virtual OptionFrameServices& options()=0;
    virtual StageResetServices& reset()=0;
    virtual int input_slot(int view)=0;
    virtual std::uint32_t held(int slot,std::uint32_t mask)=0;
    virtual bool enemy_ready(game_session::Context&)=0;
    virtual float clock_scale()=0;
    virtual void bind_script(Player&,int)=0;
    virtual sprite::Animation* animation(std::uint32_t&)=0;
    virtual std::uint32_t spawn_focus_effect(Player&)=0;
    virtual void sound(int)=0;
};
MovementServices& movement_services();
std::uint32_t set_transition_direction(Player&,std::uint32_t); //4fff10
int update_movement(Player&,MovementServices&); //4f9c30
int update_movement(Player&);
}
