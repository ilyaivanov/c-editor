#pragma once
#include "types.c"

typedef struct Spring
{
    f32 target;
    f32 velocity;
    f32 current;
    // u32 isDone;
} Spring;

f32 stiffness;
f32 damping;

void InitAnimations()
{
    stiffness = 620;
    damping = 1.7 * mysqrtf(stiffness);
}

void UpdateSpring(Spring *spring, f32 deltaTimeMs)
{
    f32 displacement = spring->target - spring->current;
    f32 springForce = displacement * stiffness;
    f32 dampingForce = spring->velocity * damping;

    spring->velocity += (springForce - dampingForce) * deltaTimeMs;
    spring->current += spring->velocity * deltaTimeMs;

    // if (Math.abs(spring.current - spring.target) < 0.1) {
    //     spring.isDone = true;
    //     spring.current = spring.target;
    // }
}
