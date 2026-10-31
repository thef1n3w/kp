#include "GameState.h"
#include <algorithm>
#include <cmath>


namespace {
void Text(Font f, const char* s, Vector2 p, float z, Color c) { p.x=roundf(p.x);p.y=roundf(p.y);DrawTextEx(f,s,p,z,0,c); }
void Center(Font f,const char*s,Rectangle r,float z,Color c){Vector2 m=MeasureTextEx(f,s,z,0);Text(f,s,{r.x+(r.width-m.x)/2,r.y+(r.height-m.y)/2},z,c);}
Rectangle Window(){float sw=GetScreenWidth(),sh=GetScreenHeight(),w=std::min(1050.f,sw-90),h=std::min(680.f,sh-70);return{(sw-w)/2,(sh-h)/2,w,h};}
Rectangle Choice(Rectangle w,int i){float gap=16,width=(w.width-160-gap*2)/3;return{w.x+80+i*(width+gap),w.y+w.height-105,width,52};}
Rectangle Close(Rectangle w){return{w.x+w.width-58,w.y+22,34,34};}
}
void ThermalChamberGame::Start(ThermalStabilityEvidence target){targetStability=target;active=true;temperature=20;exposure=0;errors=0;}
ThermalChamberResult ThermalChamberGame::Choose(ThermalStabilityEvidence c){active=false;return c==targetStability?ThermalChamberResult::Correct:ThermalChamberResult::Wrong;}
ThermalChamberResult ThermalChamberGame::Update(float delta){
    if(!active)return ThermalChamberResult::None; Rectangle w=Window();Vector2 m=GetMousePosition();
    if(IsKeyPressed(KEY_ESCAPE)||(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)&&CheckCollisionPointRec(m,Close(w)))){active=false;return ThermalChamberResult::Cancelled;}
    if(!IsResultStage()){
        if(IsKeyDown(KEY_LEFT)||IsKeyDown(KEY_A))temperature-=delta*34;
        if(IsKeyDown(KEY_RIGHT)||IsKeyDown(KEY_D))temperature+=delta*34;
        temperature=std::clamp(temperature,-20.f,60.f);
        if(temperature>=16&&temperature<=24)exposure+=delta; else exposure=std::max(0.f,exposure-delta*.7f);
        return ThermalChamberResult::None;
    }
    if(IsKeyPressed(KEY_ONE))return Choose(ThermalStabilityEvidence::Cold);
    if(IsKeyPressed(KEY_TWO))return Choose(ThermalStabilityEvidence::Heat);
    if(IsKeyPressed(KEY_THREE))return Choose(ThermalStabilityEvidence::Neutral);
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))for(int i=0;i<3;++i)if(CheckCollisionPointRec(m,Choice(w,i)))return Choose((ThermalStabilityEvidence)i);
    return ThermalChamberResult::None;
}
void ThermalChamberGame::Draw(Font font)const{
    Rectangle w=Window();Vector2 mouse=GetMousePosition();Color accent={255,155,73,255};
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),{1,3,8,226});DrawRectangleRounded(w,.035f,16,{8,17,28,252});DrawRectangleRoundedLinesEx(w,.035f,16,1.5f,{accent.r,accent.g,accent.b,130});
    Text(font,u8"ТЕРМОСТАБИЛЬНОСТЬ",{w.x+34,w.y+28},25,WHITE);
    Text(font,IsResultStage()?u8"Сравните выживаемость культуры.":u8"Стрелками удерживайте образец в зеленой зоне 3 секунды.",{w.x+34,w.y+70},15,{193,207,218,255});
    Rectangle close=Close(w);DrawRectangleRounded(close,.2f,8,{48,34,28,245});Center(font,"X",close,16,WHITE);
    Rectangle track={w.x+100,w.y+205,w.width-200,48};DrawRectangleGradientH((int)track.x,(int)track.y,(int)track.width,(int)track.height,{75,155,255,255},{255,94,62,255});
    float greenX=track.x+(16.f+20.f)/80.f*track.width;float greenW=8.f/80.f*track.width;DrawRectangleRounded({greenX,track.y-14,greenW,track.height+28},.2f,8,{90,225,145,90});DrawRectangleRoundedLinesEx({greenX,track.y-14,greenW,track.height+28},.2f,8,2,{90,225,145,255});
    float marker=track.x+(temperature+20)/80*track.width;DrawLineEx({marker,track.y-25},{marker,track.y+track.height+25},4,WHITE);
    Center(font,TextFormat("%.1f C",temperature),{w.x,w.y+285,w.width,42},24,WHITE);
    DrawRectangleRounded({w.x+220,w.y+350,w.width-440,14},1,8,{31,47,55,255});DrawRectangleRounded({w.x+220,w.y+350,(w.width-440)*std::min(1.f,exposure/3),14},1,8,{90,225,145,255});
    if(!IsResultStage()){Center(font,u8"A / ←  холоднее     D / →  теплее",{w.x,w.y+400,w.width,40},15,{163,183,196,255});return;}
    const char* result=targetStability==ThermalStabilityEvidence::Cold?u8"Культура выжила при охлаждении":targetStability==ThermalStabilityEvidence::Heat?u8"Культура выдержала нагрев":u8"Культура стабильна только в среднем диапазоне";
    Center(font,result,{w.x,w.y+410,w.width,42},17,{255,220,184,255});
    const char* labels[]={u8"1 • ХОЛОД",u8"2 • ТЕПЛО",u8"3 • НЕЙТРАЛЬНАЯ"};
    for(int i=0;i<3;++i){Rectangle r=Choice(w,i);bool h=CheckCollisionPointRec(mouse,r);DrawRectangleRounded(r,.15f,9,h?Color{accent.r,accent.g,accent.b,170}:Color{28,38,52,245});Center(font,labels[i],r,14,WHITE);}
}
