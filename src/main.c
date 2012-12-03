#include <taa/scenefile.h>
#include <taa/path.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(NDEBUG) && defined(_MSC_FULL_VER)
#include <crtdbg.h>
#include <float.h>
#endif

void export_obj(
    taa_scene* scene,
    int isanim,
    float time,
    FILE* fp);

#define MAIN_USAGE \
    "Required arguments\n" \
    "  -anim               Export meshes with animation deformation\n" \
    "  -o [FILE]           Path to output obj file\n" \
    "  -scene [FILE]       Path to input taascene file\n" \
    "  -static             Export meshes without animation deformation\n" \
    "  -time [FLOAT]       If -anim, time in seconds to export the frame\n" \
    "\n" \
    "Options\n" \
    "  -up [Y|Z]           Up axis for exported obj file\n"

typedef struct main_args_s main_args;

struct main_args_s
{
    int isanim;
    float time;
    taa_scene_upaxis upaxis;
    const char* out;
    const char* scene;
};

//****************************************************************************
static int main_parse_args(
    int argc,
    char* argv[],
    main_args* args_out)
{
    int err = 0;
    char** argitr = argv;
    char** argend = argitr + argc;
    // initialize output
    args_out->isanim = -1;
    args_out->time   = -1.0f;
    args_out->upaxis = taa_SCENE_Y_UP;
    args_out->out    = NULL;
    args_out->scene  = NULL;
    // skip past argv[0]
    ++argitr; 
    // loop
    while(argitr != argend)
    {
        const char* arg = *argitr;
        if(!strcmp(arg, "-anim"))
        {
            args_out->isanim = 1;
            ++argitr;
        }
        else if(!strcmp(arg, "-o"))
        {
            if(argitr+1 == argend)
            {
                err = -1;
                break;
            }
            args_out->out = argitr[1];
            argitr += 2;
        }
        else if(!strcmp(arg, "-scene"))
        {
            if(argitr+1 == argend)
            {
                err = -1;
                break;
            }
            args_out->scene = argitr[1];
            argitr += 2;
        }
        else if(!strcmp(arg, "-static"))
        {
            args_out->isanim = 0;
            ++argitr;
        }
        else if(!strcmp(arg, "-time"))
        {
            if(argitr+1 == argend)
            {
                err = -1;
                break;
            }
            if(sscanf(argitr[1], "%f", &args_out->time) != 1)
            {
                err = -1;
                break;
            }
            argitr += 2;
        }
        else if(!strcmp(arg, "-up"))
        {
            if(argitr+1 == argend)
            {
                err = -1;
                break;
            }
            if(toupper(argitr[1][0]) == 'Z')
            {
                args_out->upaxis = taa_SCENE_Z_UP;
            }
            argitr += 2;
        }
        else
        {
            err = -1;
            break;
        }
    }
    if(args_out->isanim < 0)
    {
        err = -1;
    }
    if(args_out->isanim == 1 && args_out->time < 0.0f)
    {
        err = -1;
    }
    if(args_out->out==NULL || args_out->scene==NULL)
    {
        err = -1;
    }
    return err;
}

int main(int argc, char* argv[])
{
    int err = 0;
    main_args args;
    taa_scene scene;
    FILE* fp;

#if !defined(NDEBUG) && defined(_MSC_FULL_VER)
    {
        uint32_t cw;
        _clearfp();
        cw = _controlfp(0, 0);
        cw &=~(EM_OVERFLOW|EM_ZERODIVIDE|EM_DENORMAL|EM_INVALID);
        _controlfp(cw, MCW_EM);
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);
        //_CrtSetBreakAlloc(0);
    }
#endif
    err = main_parse_args(argc, argv, &args);
    if(err != 0)
    {
        puts(MAIN_USAGE);
    }
    taa_scene_create(&scene, taa_SCENE_Y_UP);
    if(err == 0)
    {
        // open input file
        fp = fopen(args.scene, "rb");
        if(fp == NULL)
        {
            printf("could not open input file %s\n", args.scene);
            err = -1;
        }
    }
    if(err == 0)
    {
        // read file and deserialize contents
        taa_filestream infs;
        taa_filestream_create(fp, 1024 * 1024, taa_FILESTREAM_READ, &infs);
        err = taa_scenefile_deserialize(&infs, &scene);
        taa_filestream_destroy(&infs);
        fclose(fp);
        if(err != 0)
        {
            printf("error parsing scene file\n");
        }
    }
    if(err == 0)
    {
        // fix up axis
        taa_scene_convert_upaxis(&scene, args.upaxis);
    }
    if(err == 0)
    {
        // open output file
        fp = fopen(args.out, "wb");
        if(fp == NULL)
        {
            printf("could not open output file %s\n", args.out);
            err = -1;
        }
    }
    if(err == 0)
    {
        export_obj(&scene, args.isanim, args.time, fp);
        fclose(fp);
    }
    taa_scene_destroy(&scene);

#if !defined(NDEBUG) && defined(_MSC_FULL_VER)
    _CrtCheckMemory();
    _CrtDumpMemoryLeaks();
#endif
    return err;
}
