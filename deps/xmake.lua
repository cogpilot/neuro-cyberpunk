-- Xmake bindings for libneurosdk

target("libneurosdk")
    set_warnings("all")
    set_kind("static")
    set_languages("c17")
    add_defines(
        "NEUROSDK_BUILD_STATIC_LIB",
        "LIB_VERSION=\"0.1.1\"", 
        "_CRT_SECURE_NO_WARNINGS",
        "_CRT_RAND_S"
    )
    add_files("libneurosdk/src/neurosdk.c")
    add_includedirs("libneurosdk/include")
    add_includedirs("libneurosdk/include", { public = true })

    before_build(function(target)
        -- Build system is a little cursed, so we replicate script logic :P
        -- Note: this doesn't build on macOS for some reason,
        -- works when I pin mongoose to branch that was used on last passing CI
        import("devel.git")
        import("net.http")

        -- target:scriptdir() is necessary with my approach
        local sourcedir = path.join(target:scriptdir(), "libneurosdk")

        http.download("https://raw.githubusercontent.com/cesanta/mongoose/refs/heads/master/mongoose.c",
            path.join(sourcedir, "src/mongoose.c"))
        http.download("https://raw.githubusercontent.com/cesanta/mongoose/refs/heads/master/mongoose.h",
            path.join(sourcedir, "src/mongoose.h"))

        http.download("https://raw.githubusercontent.com/tinycthread/tinycthread/refs/heads/master/source/tinycthread.c",
            path.join(sourcedir, "src/tinycthread.c"))
        http.download("https://raw.githubusercontent.com/tinycthread/tinycthread/refs/heads/master/source/tinycthread.h",
            path.join(sourcedir, "src/tinycthread.h"))

        http.download("https://raw.githubusercontent.com/sheredom/json.h/refs/heads/master/json.h",
            path.join(sourcedir, "src/json.h"))

        local commit = git.lastcommit({ repodir = sourcedir })
        target:add("defines", ("LIB_BUILD_HASH=%s"):format(commit))
    end)
