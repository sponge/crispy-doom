#include "hiredis.h"
#include "p_local.h"
#include <assert.h>
#include <stdlib.h>

redisContext* c;

int Redis_Connect(const char* addr, int port) {
    c = redisConnect(addr, port);
    if (c == NULL || c->err) {
        if (c) {
            printf("Error: %s\n", c->errstr);
            return 0;
        }
        else {
            printf("Can't allocate redis context\n");
            return 0;
        }
    }

    return 1;
}

void Redis_Push_Thinkers() {
    int i = 0;
    int cmd = 0;
    redisReply* reply;

    for (thinker_t* th = thinkercap.next; th != &thinkercap; th = th->next, i++)
    {
        if (th->function.acp1 == (actionf_p1)P_MobjThinker)
        {
            mobj_t* mobj = (mobj_t*)th;
            redisAppendCommand(c, "HSET mobj_%i type %i x %i y %i z %i health %i",
                i,
                mobj->type,
                mobj->x,
                mobj->y,
                mobj->z,
                mobj->health
            );
            cmd++;
            continue;
        }
    }

    while (cmd-- > 0) {
        // consume message
        redisGetReply(c, (void**)&reply);
        freeReplyObject(reply);
    }
}

void Redis_Pull_Thinkers() {
    int mobjNum = 0;

    redisReply* reply = redisCommand(c, "get test");

    for (thinker_t* th = thinkercap.next; th != &thinkercap; th = th->next, mobjNum++)
    {
        if (th->function.acp1 == (actionf_p1)P_MobjThinker)
        {
            mobj_t* mobj = (mobj_t*)th;

            redisReply* reply = redisCommand(c, "HGETALL mobj_%i", mobjNum);
            assert(reply != NULL && !c->err && reply->type == REDIS_REPLY_ARRAY);

            if (reply->type == REDIS_REPLY_ERROR) {
                printf("Error: %s\n", reply->str);
            }
            else if (reply->type != REDIS_REPLY_ARRAY) {
                printf("Unexpected type: %d\n", reply->type);
            }
            else {
                int i;
                for (i = 0; i < reply->elements; i = i + 2) {
                    const char* key = reply->element[i]->str;
                    redisReply* val = reply->element[i+1];
                    if (!strcmp("type", key)) {
                        int ival = atoi(val->str);
                        if (ival != mobj->type) {
                            mobj->type = ival;
                            mobj->info = &mobjinfo[mobj->type];
                            mobj->state = &states[mobj->info->spawnstate];
                            mobj->sprite = mobj->state->sprite;
                            mobj->frame = mobj->state->frame;
                        }
                    }
                    else if (!strcmp("x", key)) {
                        mobj->x = atoi(val->str);
                    }
                    else if (!strcmp("y", key)) {
                        mobj->y = atoi(val->str);
                    }
                    else if (!strcmp("z", key)) {
                        mobj->z = atoi(val->str);
                    }
                    else if (!strcmp("health", key)) {
                        mobj->health = atoi(val->str);
                        if (mobj->type == MT_PLAYER) {
                            mobj->player->health = atoi(val->str);
                        }
                    }
                }
            }
            freeReplyObject(reply);
            continue;
        }
    }
}