/*
 * log functions
 * Copyright (c) 2003 Michel Bardiaux
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * logging functions
 */

#ifndef _MSC_VER
#include <unistd.h>
#endif

#include <stdlib.h>
#include "avutil.h"
#include "log.h"

#ifdef XBMC_360
#if LIBAVUTIL_VERSION_MAJOR > 50
static
#endif
int av_log_level = AV_LOG_INFO;
#else
static int av_log_level = AV_LOG_INFO;
#endif

static int flags;

#if defined(_WIN32) && !defined(__MINGW32CE__)
#ifndef _XBOX
#include <windows.h>
#else
#include <xtl.h>
#endif

static const uint8_t color[] = {12,12,12,14,7,7,7};

#ifdef XBMC_360
#define set_color(x)  fprintf(stderr, "\033[%d;3%dm", color[x]>>4, color[x]&15)
#define reset_color() fprintf(stderr, "\033[0m")
#endif

static int16_t background, attr_orig;
static HANDLE con;

#ifndef _XBOX
#define set_color(x)  SetConsoleTextAttribute(con, background | color[x])
#define reset_color() SetConsoleTextAttribute(con, attr_orig)
#endif
#else
static const uint8_t color[]={0x41,0x41,0x11,0x03,9,9,9};
#define set_color(x)  fprintf(stderr, "\033[%d;3%dm", color[x]>>4, color[x]&15)
#define reset_color() fprintf(stderr, "\033[0m")
#endif

static int use_color=-1;

#ifdef _XBOX
#undef _WIN32
#endif

#undef fprintf

#ifdef _XBOX
static void colored_fputs(int level, const char *str){
#ifndef XBMC_360
#ifndef _XBOX
    if(use_color<0){
#if defined(_WIN32) && !defined(__MINGW32CE__)
        CONSOLE_SCREEN_BUFFER_INFO con_info;
        con = GetStdHandle(STD_ERROR_HANDLE);
        use_color = (con != INVALID_HANDLE_VALUE) && !getenv("NO_COLOR") && !getenv("FFMPEG_FORCE_NOCOLOR");
        if (use_color) {
            GetConsoleScreenBufferInfo(con, &con_info);
            attr_orig  = con_info.wAttributes;
            background = attr_orig & 0xF0;
        }
#elif HAVE_ISATTY
        use_color= !getenv("NO_COLOR") && !getenv("FFMPEG_FORCE_NOCOLOR") &&
            (getenv("TERM") && isatty(2) || getenv("FFMPEG_FORCE_COLOR"));
#else
        use_color= getenv("FFMPEG_FORCE_COLOR") && !getenv("NO_COLOR") && !getenv("FFMPEG_FORCE_NOCOLOR");
#endif
    }

    if(use_color){
        set_color(level);
    }
#endif
    fputs(str, stderr);
#ifndef _XBOX
    if(use_color){
        reset_color();
    }
#endif
#else
    if(use_color<0){
#if HAVE_ISATTY && !defined(_WIN32)
        use_color= getenv("TERM") && !getenv("NO_COLOR") && isatty(2);
#else
        use_color= 0;
#endif

    }

    if(use_color){
        set_color(level);
    }
    fputs(str, stderr);
    if(use_color){
        reset_color();
    }
#endif
}
#endif
const char* av_default_item_name(void* ptr){
    return (*(AVClass**)ptr)->class_name;
}

static void sanitize(uint8_t *line){
    while(*line){
        if(*line < 0x08 || (*line > 0x0D && *line < 0x20))
            *line='?';
        line++;
    }
}

void av_log_default_callback(void* ptr, int level, const char* fmt, va_list vl)
{
    static int print_prefix=1;
    static int count;
#ifdef XBMC_360
    static char prev[1024];
    char line[1024];
#else
    static char prev[1024], line[1024];
#endif
    static int is_atty;
    AVClass* avc= ptr ? *(AVClass**)ptr : NULL;
    if(level>av_log_level)
        return;
#ifdef XBMC_360
    line[0]=0;
#endif
 
#undef fprintf
    if(print_prefix && avc) {
#ifdef XBMC_360
        if(avc->version >= (50<<16 | 15<<8 | 3) && avc->parent_log_context_offset){
            AVClass** parent= *(AVClass***)(((uint8_t*)ptr) + avc->parent_log_context_offset);
            if(parent && *parent){
                snprintf(line, sizeof(line), "[%s @ %p] ", (*parent)->item_name(parent), parent);
            }
        }
        snprintf(line + strlen(line), sizeof(line) - strlen(line), "[%s @ %p] ", avc->item_name(ptr), ptr);
    }
#else
        snprintf(line, sizeof(line), "[%s @ %p]", avc->item_name(ptr), ptr);
    }else
        line[0]=0;
#endif
    vsnprintf(line + strlen(line), sizeof(line) - strlen(line), fmt, vl);

    print_prefix= line[strlen(line)-1] == '\n';
#ifdef XBMC_360
#if HAVE_ISATTY
    if(!is_atty) is_atty= isatty(2) ? 1 : -1;
#endif

    if(print_prefix && (flags & AV_LOG_SKIP_REPEATED) && !strcmp(line, prev)){
#else
    if(print_prefix && !strcmp(line, prev)){
#endif
        count++;
#ifdef XBMC_360
        if(is_atty==1)
            fprintf(stderr, "    Last message repeated %d times\r", count);
#endif
        return;
    }
    if(count>0){
        fprintf(stderr, "    Last message repeated %d times\n", count);
        count=0;
    }
#ifdef XBMC_360
    sanitize(line);
    colored_fputs(av_clip(level>>3, 0, 6), line);
#else
    colored_fputs(color[av_clip(level>>3, 0, 6)], line);
#endif
    strcpy(prev, line);

}

static void (*av_log_callback)(void*, int, const char*, va_list) = av_log_default_callback;

void av_log(void* avcl, int level, const char *fmt, ...)
{
#if defined(_CONSOLE)
    AVClass* avc= avcl ? *(AVClass**)avcl : NULL;
    va_list vl;
    va_start(vl, fmt);
    if(avc && avc->version >= (50<<16 | 15<<8 | 2) && avc->log_level_offset_offset && level>=AV_LOG_FATAL)
        level += *(int*)(((uint8_t*)avcl) + avc->log_level_offset_offset);
    av_vlog(avcl, level, fmt, vl);
    va_end(vl);
#endif
}

void av_vlog(void* avcl, int level, const char *fmt, va_list vl)
{
    av_log_callback(avcl, level, fmt, vl);
}

int av_log_get_level(void)
{
    return av_log_level;
}

void av_log_set_level(int level)
{
    av_log_level = level;
}

void av_log_set_flags(int arg)
{
    flags= arg;
}

void av_log_set_callback(void (*callback)(void*, int, const char*, va_list))
{
    av_log_callback = callback;
}