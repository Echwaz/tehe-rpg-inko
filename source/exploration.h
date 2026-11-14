#pragma once
#include "exploration_data.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace exploration {
struct Point { float x=0,z=0; Point()=default; Point(float a,float b):x(a),z(b){} };
inline float distance(Point a,Point b) {return std::hypot(a.x-b.x,a.z-b.z);}
inline float length(float x,float z) {return std::hypot(x,z);}
inline bool clear(Point p,float radius) {
    for (const auto& b:exploration_data::obstacles) {
        const float x=std::max(b.x-b.hx,std::min(b.x+b.hx,p.x));
        const float z=std::max(b.z-b.hz,std::min(b.z+b.hz,p.z));
        if (length(p.x-x,p.z-z)<radius) return false;
    }
    return true;
}
inline bool onStreet(Point p,float radius) {
    const auto& road=exploration_data::road;
    for (unsigned i=1;i<sizeof road/sizeof road[0];++i) {
        const auto a=road[i-1],b=road[i];
        const float low=std::min(a.z,b.z),high=std::max(a.z,b.z);
        if (p.z<low+radius || p.z>high-radius) continue;
        const float x=a.x+(b.x-a.x)*(p.z-a.z)/(b.z-a.z);
        if (std::abs(p.x-x)<=7.4f-radius) return true;
    }
    // Overlap the segment joins, so radius checks do not create invisible seams.
    for (unsigned i=1;i+1<sizeof road/sizeof road[0];++i)
        if (std::abs(p.z-road[i].z)<=radius+.1f && std::abs(p.x-road[i].x)<7.f-radius) return true;
    if (p.x>=13.5f+radius && p.x<=20.5f-radius && p.z>=-35.f+radius && p.z<=-5.f-radius) return true;
    for (float z:{-8.f,-32.f})
        if (p.x>=1.f+radius && p.x<=20.5f-radius && std::abs(p.z-z)<=4.f-radius) return true;
    return false;
}
inline bool canStand(Point p,float radius) {return onStreet(p,radius)&&clear(p,radius);}
inline float floorHeight(Point p) {
    const auto& road=exploration_data::road;
    for(unsigned i=1;i<sizeof road/sizeof road[0];++i) {
        const auto a=road[i-1],b=road[i];
        if(p.z<std::min(a.z,b.z) || p.z>std::max(a.z,b.z))continue;
        const float x=a.x+(b.x-a.x)*(p.z-a.z)/(b.z-a.z);
        if(std::abs(p.x-x)<=4.5f)return .04f;
    }
    if(p.x>=15 && p.x<=19 && p.z>=-32 && p.z<=-8)return .04f;
    for(float z:{-8.f,-32.f})if(p.x>=2 && p.x<=20 && std::abs(p.z-z)<=2.8f)return .04f;
    return .18f;
}
inline Point move(Point p,float x,float z,float radius) {
    const int steps=std::max(1,static_cast<int>(std::ceil(length(x,z)/.12f)));
    x/=steps;z/=steps;
    for (int i=0;i<steps;++i) {
        Point q(p.x+x,p.z);if(canStand(q,radius))p=q;
        q=Point(p.x,p.z+z);if(canStand(q,radius))p=q;
    }
    return p;
}
inline bool sight(Point a,Point b) {
    const int steps=std::max(1,static_cast<int>(std::ceil(distance(a,b)/.5f)));
    for(int i=1;i<steps;++i) {
        const float t=static_cast<float>(i)/steps;
        if(!clear(Point(a.x+(b.x-a.x)*t,a.z+(b.z-a.z)*t),.08f))return false;
    }
    return true;
}
struct Patrol {
    Point position;
    float heading=0;
    int waypoint=1;
    bool alive=true,chasing=false;
};
struct World {
    Point player{0,35};
    float heading=0,cameraYaw=0,walkTime=0,grace=1;
    std::array<Patrol,3> enemies;
    bool moving=false;
    int pending=-1;
    World() {reset();}
    void reset() {
        player=Point(0,35);heading=cameraYaw=walkTime=0;grace=1;pending=-1;moving=false;
        for(int i=0;i<3;++i) {
            enemies[i]=Patrol();const auto p=exploration_data::patrols[i][0];
            enemies[i].position=Point(p.x,p.z);
        }
    }
    bool atBoss() const {return distance(player,Point(-2,-66))<4.f;}
    int cleared() const {int n=0;for(const auto& e:enemies)n+=!e.alive;return n;}
    void victory() {
        if(pending>=0 && pending<3)enemies[pending].alive=false;
        pending=-1;grace=2.f;moving=false;
    }
    void update(float strafe,float forward,float turn,bool run,bool interact,float dt) {
        if(pending>=0)return;
        dt=std::max(0.f,std::min(.05f,dt));
        cameraYaw+=turn*1.8f*dt;
        if(cameraYaw>3.141593f)cameraYaw-=6.283186f;
        if(cameraYaw<-3.141593f)cameraYaw+=6.283186f;
        const float magnitude=length(strafe,forward);
        if(magnitude>1){strafe/=magnitude;forward/=magnitude;}
        const float x=strafe*std::cos(cameraYaw)+forward*std::sin(cameraYaw);
        const float z=strafe*std::sin(cameraYaw)-forward*std::cos(cameraYaw);
        const Point before=player;
        player=move(player,x*(run?6.f:3.5f)*dt,z*(run?6.f:3.5f)*dt,.38f);
        moving=distance(before,player)>.0001f;
        if(moving){heading=std::atan2(x,-z);walkTime+=dt*(run?1.5f:1.f);}
        grace=std::max(0.f,grace-dt);
        for(int i=0;i<3;++i) {
            Patrol& e=enemies[i];if(!e.alive)continue;
            const float d=distance(player,e.position);
            if(grace<=0 && d<9.f && sight(player,e.position))e.chasing=true;
            if(d>15.f)e.chasing=false;
            Point target=player;
            if(!e.chasing) {
                const auto p=exploration_data::patrols[i][e.waypoint];target=Point(p.x,p.z);
                if(distance(e.position,target)<.18f)e.waypoint=(e.waypoint+1)%3;
            }
            const float dx=target.x-e.position.x,dz=target.z-e.position.z,len=length(dx,dz);
            if(len>.01f) {
                const float step=std::min(len,(e.chasing?2.7f:1.1f)*dt);
                const Point next=move(e.position,dx/len*step,dz/len*step,1.1f);
                if(distance(next,e.position)>.0001f)e.heading=std::atan2(dx,-dz);
                e.position=next;
            }
            if(grace<=0 && distance(player,e.position)<1.7f && sight(player,e.position)) {
                pending=i;moving=false;return;
            }
        }
        if(interact&&atBoss())pending=3;
    }
};
} // namespace exploration
