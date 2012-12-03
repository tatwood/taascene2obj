#include <taa/scalar.h>
#include <taa/vec3.h>
#include <taa/scene.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct pnvert_s pnvert;
typedef struct tvert_s tvert;
typedef struct jwvert_s jwvert;

struct pnvert_s
{
    taa_vec3 pos;
    taa_vec3 normal;
};

struct tvert_s
{
    taa_vec2 texcoord;
};

struct jwvert_s
{
    int32_t joints[4];
    float weights[4];
};

//****************************************************************************
static void calc_joint_transforms(
    const taa_sceneskel* skel,
    const taa_scenenode* nodes,
    taa_mat44* mats_out)
{
    int32_t i;
    int32_t iend;
    // calculate world space joint transforms
    for(i = 0, iend = skel->numjoints; i < iend; ++i)
    {
        taa_sceneskel_joint* joint = skel->joints + i;
        taa_mat44 localmat;
        taa_sceneskel_calc_transform(skel, nodes, i, &localmat);
        if(joint->parent >= 0)
        {
            taa_mat44_multiply(mats_out+joint->parent, &localmat, mats_out+i);
        }
        else
        {
            mats_out[i] = localmat;
        }
    }
}

//****************************************************************************
static void format_mesh(
    taa_scenemesh* mesh)
{
    taa_scenemesh_vertformat vf[] =
    {
        {
            "pn",
            taa_SCENEMESH_USAGE_POSITION,
            0,
            taa_SCENEMESH_VALUE_FLOAT32,
            3,
            0,
            0
        },
        {
            "pn",
            taa_SCENEMESH_USAGE_NORMAL,
            0,
            taa_SCENEMESH_VALUE_FLOAT32,
            3,
            12,
            0
        },
        {
            "t",
            taa_SCENEMESH_USAGE_TEXCOORD,
            0,
            taa_SCENEMESH_VALUE_FLOAT32,
            2,
            0,
            1
        },
        {
            "jw",
            taa_SCENEMESH_USAGE_BLENDINDEX,
            0,
            taa_SCENEMESH_VALUE_INT32,
            4,
            0,
            2
        },
        {
            "jw",
            taa_SCENEMESH_USAGE_BLENDWEIGHT,
            0,
            taa_SCENEMESH_VALUE_FLOAT32,
            4,
            16,
            2
        },
    };
    int32_t vs;
    int32_t indexoffset;
    int32_t numverts;
    indexoffset = 0;
    numverts = 0;
    vs = taa_scenemesh_find_stream(mesh,taa_SCENEMESH_USAGE_POSITION,0);
    if(vs >= 0)
    {
        taa_scenemesh_stream* posstream = mesh->vertexstreams + vs;
        indexoffset = posstream->indexmapping;
        numverts = posstream->numvertices;
    }
    if(taa_scenemesh_find_stream(mesh,taa_SCENEMESH_USAGE_NORMAL,0) < 0)
    {
        vs = taa_scenemesh_add_stream(
            mesh,
            "n",
            taa_SCENEMESH_USAGE_NORMAL,
            0,
            taa_SCENEMESH_VALUE_FLOAT32,
            3,
            12,
            indexoffset,
            numverts,
            NULL);
        // TODO: calculate normals
        memset(mesh->vertexstreams[vs].buffer,0,numverts * 12);
    }
    if(taa_scenemesh_find_stream(mesh,taa_SCENEMESH_USAGE_TEXCOORD,0) < 0)
    {
        vs = taa_scenemesh_add_stream(
            mesh,
            "t",
            taa_SCENEMESH_USAGE_TEXCOORD,
            0,
            taa_SCENEMESH_VALUE_FLOAT32,
            2,
            8,
            indexoffset,
            numverts,
            NULL);
        memset(mesh->vertexstreams[vs].buffer,0,numverts * 8);
    }
    if(taa_scenemesh_find_stream(mesh,taa_SCENEMESH_USAGE_BLENDINDEX,0) < 0)
    {
        vs = taa_scenemesh_add_stream(
            mesh,
            "j",
            taa_SCENEMESH_USAGE_BLENDINDEX,
            0,
            taa_SCENEMESH_VALUE_INT32,
            4,
            16,
            indexoffset,
            numverts,
            NULL);
        memset(mesh->vertexstreams[vs].buffer,0,numverts * 16);
    }
    if(taa_scenemesh_find_stream(mesh,taa_SCENEMESH_USAGE_BLENDWEIGHT,0) < 0)
    {
        vs = taa_scenemesh_add_stream(
            mesh,
            "w",
            taa_SCENEMESH_USAGE_BLENDWEIGHT,
            0,
            taa_SCENEMESH_VALUE_FLOAT32,
            4,
            16,
            indexoffset,
            numverts,
            NULL);
        memset(mesh->vertexstreams[vs].buffer,0,numverts * 16);
    }
    taa_scenemesh_format(mesh, vf, sizeof(vf)/sizeof(*vf));
}

//****************************************************************************
static void export_mesh(
    const taa_scenemesh* mesh,
    const taa_mat44* modelmat,
    const taa_mat44* jointmats,
    int isanim,
    uint32_t* vertctr,
    FILE* fp)
{
    const pnvert* pnv;
    const jwvert* jwv;
    const tvert* tv;
    const pnvert* pnitr;
    const pnvert* pnend;
    const jwvert* jwitr;
    const tvert* titr;
    const tvert* tend;
    const taa_scenemesh_face* faceitr;
    const taa_scenemesh_face* faceend;
    int32_t numverts;
    int32_t voff;
    voff = *vertctr;
    numverts = mesh->vertexstreams[0].numvertices;
    // set vertex pointers
    pnv = (const pnvert*) mesh->vertexstreams[0].buffer;
    tv    = (const  tvert*) mesh->vertexstreams[1].buffer;
    jwv = (const jwvert*) mesh->vertexstreams[2].buffer;
    // export object definition
    fprintf(fp, "o %s\n", mesh->name);
    fputs("\n", fp);
    // export skinned positions
    pnitr = pnv;
    pnend = pnitr + numverts;
    jwitr = jwv;
    while(pnitr != pnend)
    {
        int32_t i = 0;
        taa_vec3 p;
        taa_vec3_set(0.0f,0.0f,0.0f, &p);
        if(isanim && mesh->joints != NULL)
        {
            for(i = 0; i < 4; ++i)
            {
                taa_scenemesh_skinjoint* sj = mesh->joints + jwitr->joints[i];
                float w = jwitr->weights[i];
                taa_mat44* invbindmat = &sj->invbindmatrix;
                taa_mat44 W;
                taa_mat44 S;
                taa_vec3 v;
                taa_mat44_multiply(modelmat, jointmats + sj->animjoint, &W);
                taa_mat44_multiply(&W, invbindmat, &S);
                taa_mat44_transform_vec3(&S, &pnitr->pos, &v);
                taa_vec3_scale(&v, w, &v);
                taa_vec3_add(&p, &v, &p);
            }
        }
        else
        {
            p = pnitr->pos;
        }
        fprintf(fp, "v %f %f %f\n", p.x, p.y, p.z);
        ++pnitr;
        ++jwitr;
    }
    fputs("\n", fp);
    // export skinned normals
    pnitr = pnv;
    pnend = pnitr + numverts;
    jwitr = jwv;
    while(pnitr != pnend)
    {
        int32_t i = 0;
        taa_vec3 n;
        taa_vec3_set(0.0f,0.0f,0.0f, &n);
        if(isanim && mesh->joints != NULL)
        {
            for(i = 0; i < 4; ++i)
            {
                taa_scenemesh_skinjoint* sj = mesh->joints + jwitr->joints[i];
                float w = jwitr->weights[i];
                taa_mat44* invbindmat = &sj->invbindmatrix;
                taa_mat44 W;
                taa_mat44 S;
                taa_vec3 v;
                taa_mat44_multiply(modelmat, jointmats + sj->animjoint, &W);
                taa_mat44_multiply(&W, invbindmat, &S);
                taa_vec4_set(0.0f,0.0f,0.0f,1.0f,&S.w);
                taa_mat44_transform_vec3(&S, &pnitr->normal, &v);
                taa_vec3_scale(&v, w, &v);
                taa_vec3_add(&n, &v, &n);
            }
        }
        else
        {
            n = pnitr->normal;
        }
        taa_vec3_normalize(&n, &n);
        fprintf(fp, "vn %f %f %f\n", n.x, n.y, n.z);
        ++pnitr;
        ++jwitr;
    }
    fputs("\n", fp);
    // export texture coordinates
    titr = tv;
    tend = titr + numverts;
    while(titr != tend)
    {
        fprintf(fp, "vt %f %f\n", titr->texcoord.x, titr->texcoord.y);
        ++titr;
    }
    fputs("\n", fp);
    // export faces
    faceitr = mesh->faces;
    faceend = faceitr + mesh->numfaces;
    while(faceitr != faceend)
    {
        const uint32_t* indexitr = mesh->indices + faceitr->firstindex;
        const uint32_t* indexend = indexitr + faceitr->numindices;
        fprintf(fp, "f");
        while(indexitr != indexend)
        {
            uint32_t index = voff + (*indexitr) + 1;
            fprintf(fp, " %i/%i/%i", index, index, index);
            ++indexitr;
        }
        fputs("\n", fp);
        ++faceitr;
    }
    fputs("\n", fp);
    *vertctr = voff + numverts;
}

//****************************************************************************
static void export_skel(
    taa_sceneskel* skel,
    const taa_mat44* jointmats,
    uint32_t* vertctr,
    FILE* fp)
{
    const taa_sceneskel_joint* jitr;
    const taa_sceneskel_joint* jend;
    const taa_mat44* jointmatitr;
    int32_t i;
    int32_t voff;
    voff = *vertctr;
    // export object definition
    fprintf(fp, "o %s\n", skel->name);
    fputs("\n", fp);
    // export joint vertices (2 for each joint)
    jitr = skel->joints;
    jend = jitr + skel->numjoints;
    jointmatitr = jointmats;
    while(jitr != jend)
    {
        const taa_vec4* p = &jointmatitr->w;
        fprintf(fp, "v %f %f %f\n", p->x, p->y, p->z);
        fprintf(fp, "v %f %f %f\n", p->x, p->y, p->z);
        ++jointmatitr;
        ++jitr;
    }
    fputs("\n", fp);
    // export bone faces
    jitr = skel->joints;
    jend = jitr + skel->numjoints;
    i = 0;
    while(jitr != jend)
    {
        if(jitr->parent >= 0)
        {
            int32_t vc = voff + i*2 + 1;
            int32_t vp = voff + jitr->parent*2 + 1;
            fprintf(fp, "f %i %i %i\n", vc, vc+1, vp);
        }
        ++jitr;
        ++i;
    }
    fputs("\n", fp);
    *vertctr = voff + skel->numjoints*2;
}

//****************************************************************************
void export_obj(
    taa_scene* scene,
    int isanim,
    float time,
    FILE* fp)
{
    taa_scenenode* animnodes;
    taa_mat44** skelmats;
    int32_t i;
    int32_t numnodes;
    int32_t numskels;
    int32_t nummeshes;
    uint32_t vertctr;

    vertctr = 0;

    numnodes = scene->numnodes;
    animnodes = (taa_scenenode*) taa_memalign(
        16,
        numnodes*sizeof(*animnodes));
    memcpy(animnodes,scene->nodes, numnodes*sizeof(*animnodes));

    numskels = scene->numskeletons;
    skelmats = (taa_mat44**) malloc(numskels*sizeof(*skelmats));
    for(i = 0; i < numskels; ++i)
    {
        taa_mat44* jointmats;
        int32_t numjoints = scene->skeletons[i].numjoints;
        jointmats = (taa_mat44*)  taa_memalign(
            16,
            numjoints*sizeof(*jointmats));
        skelmats[i] = jointmats;
    }

    // prepare meshes for export
    nummeshes = scene->nummeshes;
    for(i = 0; i < nummeshes; ++i)
    {
        format_mesh(scene->meshes + i);
    }

    // update animate sqts
    if(isanim &&scene->numanimations > 0)
    {
        taa_sceneanim* anim = scene->animations;
        time = (float) (time - anim->length*floor(time/anim->length));
        taa_sceneanim_play(anim,time,animnodes,numnodes);
    }
    for(i = 0; i < numskels; ++i)
    {
        calc_joint_transforms(
            scene->skeletons + i,
            animnodes,
            skelmats[i]);
    }

    // export meshes
    for(i = 0; i < numnodes; ++i)
    {
        taa_scenenode* node = scene->nodes + i;
        if(node->type == taa_SCENENODE_REF_MESH)
        {
            taa_scenemesh* mesh;
            const taa_mat44* jointmats;
            taa_mat44 modelmat;
            taa_mat44 I;
            mesh = scene->meshes + node->value.meshid;
            taa_mat44_identity(&I);
            jointmats = &I; 
            if(mesh->skeleton >= 0)
            {
                jointmats = skelmats[mesh->skeleton];
            }
            taa_scenenode_calc_transform(animnodes, i, &modelmat);
            export_mesh(mesh, &modelmat, jointmats, isanim, &vertctr, fp);
        }
    }

    // export skeletons
    for(i = 0; i < numskels; ++i)
    {
        taa_sceneskel* skel = scene->skeletons + i;
        taa_mat44* jointmats = skelmats[i];
        export_skel(skel, jointmats, &vertctr, fp);
    }

    taa_memalign_free(animnodes);
    for(i = 0; i < numskels; ++i)
    {
        taa_memalign_free(skelmats[i]);
    }
    free(skelmats);
}
