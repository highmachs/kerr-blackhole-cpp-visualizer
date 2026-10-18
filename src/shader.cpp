#include "shader.h"
#include <cstdio>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

namespace kerr
{

    std::string readFile(const std::string &path)
    {
        std::ifstream f(path);
        if (!f)
        {
            return {};
        }
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    static bool checkShader(GLuint s, const std::string &label)
    {
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (ok)
            return true;
        GLint len = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(std::max(len, 1));
        glGetShaderInfoLog(s, len, nullptr, log.data());
        std::fprintf(stderr, "[shader:%s] compile failed:\n%s\n", label.c_str(), log.data());
        return false;
    }

    static bool checkProgram(GLuint p, const std::string &label)
    {
        GLint ok = 0;
        glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (ok)
            return true;
        GLint len = 0;
        glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(std::max(len, 1));
        glGetProgramInfoLog(p, len, nullptr, log.data());
        std::fprintf(stderr, "[shader:%s] link failed:\n%s\n", label.c_str(), log.data());
        return false;
    }

    static GLuint compile(GLenum type, const std::string &src, const std::string &label)
    {
        GLuint s = glCreateShader(type);
        const char *c = src.c_str();
        glShaderSource(s, 1, &c, nullptr);
        glCompileShader(s);
        if (!checkShader(s, label))
        {
            glDeleteShader(s);
            return 0;
        }
        return s;
    }

    GLuint loadComputeProgram(const std::string &path, const char *embeddedFallback)
    {
        std::string src = readFile(path);
        std::string label = path;
        if (src.empty())
        {
            if (embeddedFallback && embeddedFallback[0] != '\0')
            {
                src = embeddedFallback;
                label = path + " [embedded]";
                std::printf("[kerr] Using embedded compute shader for %s\n", path.c_str());
            }
            else
            {
                std::fprintf(stderr, "[kerr] Shader file missing and no embedded fallback: %s\n", path.c_str());
                return 0;
            }
        }
        GLuint cs = compile(GL_COMPUTE_SHADER, src, label);
        if (!cs)
            return 0;
        GLuint p = glCreateProgram();
        glAttachShader(p, cs);
        glLinkProgram(p);
        glDeleteShader(cs);
        if (!checkProgram(p, label))
        {
            glDeleteProgram(p);
            return 0;
        }
        return p;
    }

    GLuint loadGraphicsProgram(const std::string &vertPath, const std::string &fragPath,
                               const char *embeddedVertFallback, const char *embeddedFragFallback)
    {
        std::string vsrc = readFile(vertPath);
        std::string vlabel = vertPath;
        if (vsrc.empty())
        {
            if (embeddedVertFallback && embeddedVertFallback[0] != '\0')
            {
                vsrc = embeddedVertFallback;
                vlabel = vertPath + " [embedded]";
            }
            else
            {
                std::fprintf(stderr, "[kerr] Vertex shader missing and no fallback: %s\n", vertPath.c_str());
                return 0;
            }
        }

        std::string fsrc = readFile(fragPath);
        std::string flabel = fragPath;
        if (fsrc.empty())
        {
            if (embeddedFragFallback && embeddedFragFallback[0] != '\0')
            {
                fsrc = embeddedFragFallback;
                flabel = fragPath + " [embedded]";
            }
            else
            {
                std::fprintf(stderr, "[kerr] Fragment shader missing and no fallback: %s\n", fragPath.c_str());
                return 0;
            }
        }

        GLuint vs = compile(GL_VERTEX_SHADER, vsrc, vlabel);
        GLuint fs = compile(GL_FRAGMENT_SHADER, fsrc, flabel);
        if (!vs || !fs)
        {
            if (vs)
                glDeleteShader(vs);
            if (fs)
                glDeleteShader(fs);
            return 0;
        }
        GLuint p = glCreateProgram();
        glAttachShader(p, vs);
        glAttachShader(p, fs);
        glLinkProgram(p);
        glDeleteShader(vs);
        glDeleteShader(fs);
        if (!checkProgram(p, vlabel + "+" + flabel))
        {
            glDeleteProgram(p);
            return 0;
        }
        return p;
    }

} // namespace kerr
