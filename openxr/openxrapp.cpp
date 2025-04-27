#include "openxrapp.h"

class OpenXRApp {
    public:
        OpenXRApp() = default;
        ~OpenXRApp() {
            // Ensure cleanup happens even if Shutdown isn't explicitly called
            // Though calling Shutdown explicitly after Run is preferred.
            Shutdown();
        }
    
        // Prevent copying
        OpenXRApp(const OpenXRApp&) = delete;
        OpenXRApp& operator=(const OpenXRApp&) = delete;
    
        bool Initialize() {
            printf("Initializing OpenXR Application...\n");
    
            print_api_layers(); // Informational
    
            if (!CheckInstanceExtensions()) return false;
            if (!CreateInstance()) return false;
            if (!LoadExtensionFunctions()) return false; // Load functions after instance creation
            print_instance_properties(m_instance);
    
            if (!GetSystem()) return false;
            if (!GetViewConfigurations()) return false;
            if (!CheckGraphicsRequirements()) return false;
            if (!InitializePlatformGraphics()) return false; // Needs view config for window size
            if (!CreateSession()) return false;
            if (!CreateReferenceSpace()) return false;
            if (!CreateSwapchains()) return false;
            if (!InitializeRenderResources()) return false; // Needs swapchain info
    
            printf("Initialization Complete.\n");
            return true;
        }
    
        void Run() {
            printf("Starting Main Loop...\n");
            m_quit_mainloop = false;
            m_session_running = false;
    
            // Initial state check might be needed depending on runtime behavior
            // For simplicity, assume we start needing to handle events.
    
            while (!m_quit_mainloop) {
                PollEvents(); // Handle SDL and OpenXR events
    
                if (!m_run_framecycle) {
                     // If not running (e.g., session IDLE, STOPPING), skip frame logic
                     // Add a small sleep perhaps to avoid busy-waiting?
                     // std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
    
                if (!RenderFrame()) {
                    // If RenderFrame fails critically, exit loop
                    m_quit_mainloop = true;
                }
            }
            printf("Exiting Main Loop.\n");
        }
    
        void Shutdown() {
            printf("Shutting Down...\n");
    
            // Cleanup GL resources first while context might still be valid
            CleanupRenderResources();
            CleanupPlatformGraphics(); // Destroy window/context
    
            // Destroy OpenXR resources (order matters: dependent resources first)
            CleanupSwapchains();
    
            if (m_play_space != XR_NULL_HANDLE) {
                xrDestroySpace(m_play_space);
                m_play_space = XR_NULL_HANDLE;
            }
            // Session destruction is handled by state changes (EXITING)
            // but add a check here in case loop exited abruptly
            if (m_session != XR_NULL_HANDLE) {
                 // Don't destroy if it was already handled by EXITING state
                if (m_state != XR_SESSION_STATE_EXITING && m_state != XR_SESSION_STATE_LOSS_PENDING) {
                     printf("Force destroying session...\n");
                     xrDestroySession(m_session);
                }
                m_session = XR_NULL_HANDLE; // Ensure it's marked as null
            }
    
            if (m_instance != XR_NULL_HANDLE) {
                xrDestroyInstance(m_instance);
                m_instance = XR_NULL_HANDLE;
            }
    
            // Vectors will automatically free their memory
            printf("Shutdown Complete.\n");
        }
    
    
    private:
        // --- Configuration ---
        XrFormFactor m_form_factor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        XrViewConfigurationType m_view_type = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        XrReferenceSpaceType m_play_space_type = XR_REFERENCE_SPACE_TYPE_LOCAL;
        float m_near_z = 0.01f;
        float m_far_z = 100.0f;
    
        // --- OpenXR Handles ---
        XrInstance m_instance = XR_NULL_HANDLE;
        XrSystemId m_system_id = XR_NULL_SYSTEM_ID;
        XrSession m_session = XR_NULL_HANDLE;
        XrSpace m_play_space = XR_NULL_HANDLE;
    
        // --- Graphics Binding ---
        XrGraphicsBindingOpenGLXlibKHR m_graphics_binding_gl = {XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR}; // Initialize type
    
        // --- View & Swapchain Data ---
        uint32_t m_view_count = 0;
        std::vector<XrViewConfigurationView> m_viewconfig_views;
        std::vector<XrCompositionLayerProjectionView> m_projection_views;
        std::vector<XrView> m_views;
        std::vector<XrSwapchain> m_swapchains;
        std::vector<uint32_t> m_swapchain_lengths;
        std::vector<std::vector<XrSwapchainImageOpenGLKHR>> m_swapchain_images;
    
        // --- OpenGL Rendering Resources ---
        std::vector<std::vector<GLuint>> m_gl_framebuffers;
        GLuint m_gl_shader_program_id = 0;
        GLuint m_gl_VAO = 0;
    
        // --- Main Loop State ---
        bool m_quit_mainloop = false;
        bool m_session_running = false;
        bool m_run_framecycle = false; // Start assuming we shouldn't run until state is right
        XrSessionState m_state = XR_SESSION_STATE_UNKNOWN;
    
    
        // ========================================================================
        // Initialization Methods
        // ========================================================================
    
        bool CheckInstanceExtensions() {
            uint32_t ext_count = 0;
            XrResult result = xrEnumerateInstanceExtensionProperties(NULL, 0, &ext_count, NULL);
            // Early check without instance is tricky for xr_check, do manually
            if (result != XR_SUCCESS) {
                 printf("Failed to enumerate number of instance extensions: %d\n", result);
                 return false;
            }
    
            std::vector<XrExtensionProperties> ext_props(ext_count, {XR_TYPE_EXTENSION_PROPERTIES});
            result = xrEnumerateInstanceExtensionProperties(NULL, ext_count, &ext_count, ext_props.data());
             if (result != XR_SUCCESS) {
                 printf("Failed to enumerate instance extensions: %d\n", result);
                 return false;
            }
    
            bool opengl_supported = false;
            printf("Runtime supports %u extensions:\n", ext_count);
            for (uint32_t i = 0; i < ext_count; i++) {
                printf("\t%s v%d\n", ext_props[i].extensionName, ext_props[i].extensionVersion);
                if (strcmp(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME, ext_props[i].extensionName) == 0) {
                    opengl_supported = true;
                }
            }
    
            if (!opengl_supported) {
                printf("Runtime does not support required extension %s!\n", XR_KHR_OPENGL_ENABLE_EXTENSION_NAME);
                return false;
            }
            return true;
        }
    
        bool CreateInstance() {
            std::vector<const char*> enabled_exts = {XR_KHR_OPENGL_ENABLE_EXTENSION_NAME};
    
            XrInstanceCreateInfo instance_create_info = {XR_TYPE_INSTANCE_CREATE_INFO}; // Use designated initializer or zero-fill
            instance_create_info.createFlags = 0;
            instance_create_info.enabledExtensionCount = static_cast<uint32_t>(enabled_exts.size());
            instance_create_info.enabledExtensionNames = enabled_exts.data();
            instance_create_info.enabledApiLayerCount = 0;
            instance_create_info.enabledApiLayerNames = NULL;
            instance_create_info.applicationInfo = {}; // Zero initialize
            strncpy(instance_create_info.applicationInfo.applicationName, "OpenXR C++ Example", XR_MAX_APPLICATION_NAME_SIZE -1);
            strncpy(instance_create_info.applicationInfo.engineName, "Custom C++", XR_MAX_ENGINE_NAME_SIZE - 1);
            instance_create_info.applicationInfo.applicationVersion = 1;
            instance_create_info.applicationInfo.engineVersion = 1;
            instance_create_info.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    
            XrResult result = xrCreateInstance(&instance_create_info, &m_instance);
            // Instance is NULL here, so xr_check might fail to convert result code. Check manually first.
            if (result != XR_SUCCESS) {
                 printf("Failed to create XR instance: %d\n", result);
                 return false;
            }
            // Now call xr_check which might provide more details if configured
            if (!xr_check(m_instance, result, "Failed to create XR instance.")) {
                 // Cleanup potentially partially created instance? OpenXR spec is unclear here.
                 // Best practice: if xrCreateInstance fails, assume m_instance is invalid.
                 m_instance = XR_NULL_HANDLE;
                 return false;
            }
    
            printf("XR Instance created successfully.\n");
            return true;
        }
    
        bool LoadExtensionFunctions() {
            // Assumes load_extension_function_pointers takes the instance
            // and loads necessary function pointers (like pfnGetOpenGLGraphicsRequirementsKHR)
            // into global scope or makes them accessible somehow.
             if (!load_extension_function_pointers(m_instance)) {
                printf("Failed to load OpenXR extension function pointers.\n");
                return false;
             }
             // Verify essential pointers are loaded (example)
             if (pfnGetOpenGLGraphicsRequirementsKHR == nullptr) {
                 printf("Required function pfnGetOpenGLGraphicsRequirementsKHR not loaded.\n");
                 return false;
             }
             return true;
        }
    
    
        bool GetSystem() {
            XrSystemGetInfo system_get_info = {XR_TYPE_SYSTEM_GET_INFO};
            system_get_info.formFactor = m_form_factor;
    
            XrResult result = xrGetSystem(m_instance, &system_get_info, &m_system_id);
            if (!xr_check(m_instance, result, "Failed to get system for form factor %d.", m_form_factor)) {
                return false;
            }
    
            printf("Successfully got XrSystem with id %lu for form factor %d\n", m_system_id, m_form_factor);
    
            // Get and print system properties
            XrSystemProperties system_props = {XR_TYPE_SYSTEM_PROPERTIES};
            result = xrGetSystemProperties(m_instance, m_system_id, &system_props);
            if (!xr_check(m_instance, result, "Failed to get System properties")) {
                return false; // Or just print warning?
            }
            print_system_properties(system_props);
            return true;
        }
    
        bool GetViewConfigurations() {
             XrResult result = xrEnumerateViewConfigurationViews(m_instance, m_system_id, m_view_type, 0, &m_view_count, NULL);
             if (!xr_check(m_instance, result, "Failed to get view configuration view count!")) return false;
    
             m_viewconfig_views.resize(m_view_count, {XR_TYPE_VIEW_CONFIGURATION_VIEW}); // Resize and initialize type
    
             result = xrEnumerateViewConfigurationViews(m_instance, m_system_id, m_view_type, m_view_count, &m_view_count, m_viewconfig_views.data());
             if (!xr_check(m_instance, result, "Failed to enumerate view configuration views!")) return false;
    
             print_viewconfig_view_info(m_view_count, m_viewconfig_views);
    
             // Pre-allocate other view-related vectors now that we know view_count
             m_views.resize(m_view_count, {XR_TYPE_VIEW});
             m_projection_views.resize(m_view_count, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});
    
             return true;
        }
    
         bool CheckGraphicsRequirements() {
            XrGraphicsRequirementsOpenGLKHR opengl_reqs = {XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
            if (!pfnGetOpenGLGraphicsRequirementsKHR) {
                printf("pfnGetOpenGLGraphicsRequirementsKHR function pointer is null!\n");
                return false;
            }
            XrResult result = pfnGetOpenGLGraphicsRequirementsKHR(m_instance, m_system_id, &opengl_reqs);
            if (!xr_check(m_instance, result, "Failed to get OpenGL graphics requirements!")) {
                return false;
            }
            // Optionally check opengl_reqs.minApiVersionSupported / maxApiVersionSupported here
            printf("OpenGL graphics requirements checked.\n"); // Min: %lx, Max: %lx\n", opengl_reqs.minApiVersionSupported, opengl_reqs.maxApiVersionSupported);
            return true;
         }
    
        bool InitializePlatformGraphics() {
            // Assumes viewconfig_views is populated
            if (m_view_count == 0 || m_viewconfig_views.empty()) {
                printf("Cannot initialize platform graphics: View configuration not ready.\n");
                return false;
            }
    
            // Using the first view's recommended size for the companion window
            if (!init_sdl_window(&m_graphics_binding_gl.xDisplay,
                                 &m_graphics_binding_gl.visualid,
                                 &m_graphics_binding_gl.glxFBConfig,
                                 &m_graphics_binding_gl.glxDrawable,
                                 &m_graphics_binding_gl.glxContext,
                                 m_viewconfig_views[0].recommendedImageRectWidth,
                                 m_viewconfig_views[0].recommendedImageRectHeight)) {
                printf("Platform graphics (SDL/GLX) init failed!\n");
                return false;
            }
    
            printf("Using OpenGL version: %s\n", glGetString(GL_VERSION));
            printf("Using OpenGL Renderer: %s\n", glGetString(GL_RENDERER));
    
            // This might be needed for some drivers/runtimes before calling GL functions
             glXMakeCurrent(m_graphics_binding_gl.xDisplay, m_graphics_binding_gl.glxDrawable, m_graphics_binding_gl.glxContext);
    
            return true;
        }
    
        bool CreateSession() {
            XrSessionCreateInfo session_create_info = {XR_TYPE_SESSION_CREATE_INFO};
            session_create_info.next = &m_graphics_binding_gl; // Link the graphics binding
            session_create_info.systemId = m_system_id;
    
            XrResult result = xrCreateSession(m_instance, &session_create_info, &m_session);
            if (!xr_check(m_instance, result, "Failed to create session")) {
                return false;
            }
    
            printf("Successfully created a session with OpenGL!\n");
            return true;
        }
    
    
        bool CreateReferenceSpace() {
            XrReferenceSpaceCreateInfo play_space_create_info = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
            play_space_create_info.referenceSpaceType = m_play_space_type;
            play_space_create_info.poseInReferenceSpace = identity_pose; // Assumes identity_pose is defined
    
            XrResult result = xrCreateReferenceSpace(m_session, &play_space_create_info, &m_play_space);
            if (!xr_check(m_instance, result, "Failed to create play space type %d", m_play_space_type)) {
                return false;
            }
            printf("Reference space created.\n");
            return true;
        }
    
        bool CreateSwapchains() {
            uint32_t format_count;
            XrResult result = xrEnumerateSwapchainFormats(m_session, 0, &format_count, NULL);
            if (!xr_check(m_instance, result, "Failed to get swapchain format count")) return false;
    
            std::vector<int64_t> formats(format_count);
            result = xrEnumerateSwapchainFormats(m_session, format_count, &format_count, formats.data());
            if (!xr_check(m_instance, result, "Failed to enumerate swapchain formats")) return false;
    
            // Choose a format (e.g., SRGB)
            // GL_SRGB8_ALPHA8_EXT needs the appropriate GL header defining it
            #ifndef GL_SRGB8_ALPHA8_EXT
            #define GL_SRGB8_ALPHA8_EXT 0x8C43 // Define if missing
            #endif
            int64_t color_format = get_swapchain_format(m_instance, m_session, GL_SRGB8_ALPHA8_EXT, true);
             if (color_format == 0) { // Assuming 0 indicates failure or no suitable format
                 printf("Failed to find suitable swapchain format.\n");
                 return false;
             }
    
    
            // Allocate space for swapchain handles and metadata
            m_swapchains.resize(m_view_count);
            m_swapchain_lengths.resize(m_view_count);
            m_swapchain_images.resize(m_view_count);
    
            for (uint32_t i = 0; i < m_view_count; ++i) {
                XrSwapchainCreateInfo swapchain_create_info = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
                swapchain_create_info.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
                swapchain_create_info.format = color_format;
                swapchain_create_info.sampleCount = m_viewconfig_views[i].recommendedSwapchainSampleCount;
                swapchain_create_info.width = m_viewconfig_views[i].recommendedImageRectWidth;
                swapchain_create_info.height = m_viewconfig_views[i].recommendedImageRectHeight;
                swapchain_create_info.faceCount = 1;
                swapchain_create_info.arraySize = 1;
                swapchain_create_info.mipCount = 1;
    
                result = xrCreateSwapchain(m_session, &swapchain_create_info, &m_swapchains[i]);
                if (!xr_check(m_instance, result, "Failed to create swapchain %d!", i)) return false;
    
                // Enumerate images
                result = xrEnumerateSwapchainImages(m_swapchains[i], 0, &m_swapchain_lengths[i], NULL);
                if (!xr_check(m_instance, result, "Failed to enumerate swapchain images count for view %d", i)) return false;
    
                m_swapchain_images[i].resize(m_swapchain_lengths[i], {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR}); // Resize and set type
    
                result = xrEnumerateSwapchainImages(m_swapchains[i], m_swapchain_lengths[i], &m_swapchain_lengths[i],
                                                    (XrSwapchainImageBaseHeader*)m_swapchain_images[i].data());
                if (!xr_check(m_instance, result, "Failed to enumerate swapchain images for view %d", i)) return false;
    
    
                // --- Setup the corresponding projection view ---
                // Most fields are constant, pose and fov updated per frame
                m_projection_views[i].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW; // Ensure type is set
                m_projection_views[i].subImage.swapchain = m_swapchains[i];
                m_projection_views[i].subImage.imageArrayIndex = 0;
                m_projection_views[i].subImage.imageRect.offset = {0, 0};
                m_projection_views[i].subImage.imageRect.extent = {
                    (int32_t)m_viewconfig_views[i].recommendedImageRectWidth,
                    (int32_t)m_viewconfig_views[i].recommendedImageRectHeight
                };
            }
    
            printf("Swapchains created successfully.\n");
            return true;
        }
    
         bool InitializeRenderResources() {
             // Make sure GL context is current
             glXMakeCurrent(m_graphics_binding_gl.xDisplay, m_graphics_binding_gl.glxDrawable, m_graphics_binding_gl.glxContext);
    
             // init_gl needs adapting to use std::vector for framebuffers
             if (init_gl(m_view_count, m_swapchain_lengths, m_gl_framebuffers, m_gl_shader_program_id, m_gl_VAO) != 0) {
                 printf("OpenGL resource initialization (shaders, FBOs) failed!\n");
                 return false;
             }
             printf("OpenGL render resources initialized.\n");
             return true;
         }
    
    
        // ========================================================================
        // Main Loop Methods
        // ========================================================================
    
        void PollEvents() {
            PollSdlEvents(); // Check for window close, ESC key, etc.
    
            // Poll OpenXR events
            XrEventDataBuffer runtime_event = {XR_TYPE_EVENT_DATA_BUFFER}; // Needs to be reset or use a new one each poll loop
            while (true) {
                runtime_event = {XR_TYPE_EVENT_DATA_BUFFER}; // Re-initialize type for safety
                XrResult poll_result = xrPollEvent(m_instance, &runtime_event);
    
                if (poll_result == XR_SUCCESS) {
                    ProcessEvent(runtime_event);
                } else if (poll_result == XR_EVENT_UNAVAILABLE) {
                    // No more events in the queue
                    break;
                } else {
                    printf("xrPollEvent failed! Result: %d\n", poll_result);
                    // Consider this a fatal error?
                    m_quit_mainloop = true;
                    break;
                }
            }
        }
    
        void PollSdlEvents() {
            SDL_Event sdl_event;
            while (SDL_PollEvent(&sdl_event)) {
                if (sdl_event.type == SDL_QUIT ||
                    (sdl_event.type == SDL_KEYDOWN && sdl_event.key.keysym.sym == SDLK_ESCAPE))
                {
                    printf("Exit requested via SDL event...\n");
                    // Request OpenXR session exit. This will trigger state changes handled in PollEvents.
                    if (m_session != XR_NULL_HANDLE) {
                       xrRequestExitSession(m_session);
                    } else {
                       // If session doesn't exist yet, just quit the loop directly
                       m_quit_mainloop = true;
                    }
                }
                // Handle other SDL events if needed
            }
        }
    
    
        void ProcessEvent(const XrEventDataBuffer& event) {
            switch (event.type) {
                case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
                    auto* ev = reinterpret_cast<const XrEventDataInstanceLossPending*>(&event);
                    printf("EVENT: Instance loss pending at %lu! Exiting loop.\n", ev->lossTime);
                    m_quit_mainloop = true; // Instance is going away, can't recover
                    break;
                }
                case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                    auto* ev = reinterpret_cast<const XrEventDataSessionStateChanged*>(&event);
                     printf("EVENT: Session state changed from %d to %d (session running: %d)\n", m_state, ev->state, m_session_running);
                    HandleSessionStateChanged(ev);
                    break;
                }
                case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED: {
                    printf("EVENT: Interaction profile changed!\n");
                    HandleInteractionProfileChanged();
                    break;
                }
                // Handle other events like XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING if needed
                default: {
                    printf("Unhandled event type: %d\n", event.type);
                    break;
                }
            }
        }
    
        void HandleSessionStateChanged(const XrEventDataSessionStateChanged* event) {
            XrSessionState old_state = m_state;
            m_state = event->state;
    
            switch (m_state) {
                case XR_SESSION_STATE_READY: {
                    if (!m_session_running) {
                        XrSessionBeginInfo session_begin_info = {XR_TYPE_SESSION_BEGIN_INFO};
                        session_begin_info.primaryViewConfigurationType = m_view_type;
                        XrResult res = xrBeginSession(m_session, &session_begin_info);
                        if (xr_check(m_instance, res, "Failed to begin session!")) {
                            printf("Session begun.\n");
                            m_session_running = true;
                            m_run_framecycle = true; // Start rendering
                        } else {
                            // Failed to begin session, critical error?
                            m_quit_mainloop = true;
                        }
                    } else {
                         m_run_framecycle = true; // Ensure frame cycle runs if already running (e.g. coming from focused)
                    }
                    break;
                }
                case XR_SESSION_STATE_STOPPING: {
                    if (m_session_running) {
                        printf("Session stopping...\n");
                        // We should have received this state because we called xrRequestExitSession OR
                        // the runtime initiated it. Now we MUST call xrEndSession.
                        XrResult res = xrEndSession(m_session);
                         if (!xr_check(m_instance, res, "Failed to end session!")) {
                             // Problem ending session, but proceed to stop rendering anyway?
                         }
                         printf("Session ended.\n");
                         m_session_running = false;
    
                    }
                     m_run_framecycle = false; // Stop rendering
                    break;
                }
                case XR_SESSION_STATE_EXITING:
                case XR_SESSION_STATE_LOSS_PENDING: {
                     printf("Session exiting or lost. Destroying session and quitting.\n");
                     // Session is ending permanently, destroy it.
                     // No need to call xrEndSession if we are EXITING/LOSS_PENDING
                     if (m_session != XR_NULL_HANDLE) {
                          XrResult res = xrDestroySession(m_session);
                           xr_check(m_instance, res, "Failed to destroy session!"); // Log error but continue cleanup
                           m_session = XR_NULL_HANDLE; // Mark as destroyed
                     }
                     m_run_framecycle = false;
                     m_quit_mainloop = true; // Exit the application's main loop
                    break;
                }
                 case XR_SESSION_STATE_IDLE: {
                    printf("Session is idle. Stopping frame cycle.\n");
                    m_run_framecycle = false; // Stop rendering, but keep polling events
                    break;
                 }
                case XR_SESSION_STATE_FOCUSED:
                case XR_SESSION_STATE_VISIBLE:
                case XR_SESSION_STATE_SYNCHRONIZED: {
                     // These states indicate we should be rendering.
                     // Ensure the frame cycle runs, especially if coming from IDLE.
                     if (!m_run_framecycle) {
                         printf("Session is synchronized/visible/focused. Starting frame cycle.\n");
                     }
                     m_run_framecycle = true;
                     break;
                }
                case XR_SESSION_STATE_UNKNOWN:
                default: {
                     printf("Entered unknown session state %d. Stopping frame cycle.\n", m_state);
                     m_run_framecycle = false;
                     break; // Or handle as error?
                }
            }
        }
    
        void HandleInteractionProfileChanged() {
             // Example implementation
             XrInteractionProfileState state = {XR_TYPE_INTERACTION_PROFILE_STATE};
        }
    
        // ========================================================================
        // Frame Rendering Methods
        // ========================================================================
    
        bool RenderFrame() {
            XrFrameState frame_state = {XR_TYPE_FRAME_STATE};
            XrFrameWaitInfo frame_wait_info = {XR_TYPE_FRAME_WAIT_INFO};
            XrResult result = xrWaitFrame(m_session, &frame_wait_info, &frame_state);
            if (!xr_check(m_instance, result, "xrWaitFrame() failed!")) return false; // Critical failure
    
            // --- Begin Frame ---
            XrFrameBeginInfo frame_begin_info = {XR_TYPE_FRAME_BEGIN_INFO};
            result = xrBeginFrame(m_session, &frame_begin_info);
            if (!xr_check(m_instance, result, "xrBeginFrame() failed!")) return false; // Critical failure
    
    
            // --- Locate Views ---
            // Only locate views if we intend to render. Located views are only valid between Begin/End frame.
            bool located_views = false;
            XrViewState view_state = {XR_TYPE_VIEW_STATE}; // Store view state flags
            if (frame_state.shouldRender) {
                XrViewLocateInfo view_locate_info = {XR_TYPE_VIEW_LOCATE_INFO};
                view_locate_info.viewConfigurationType = m_view_type;
                view_locate_info.displayTime = frame_state.predictedDisplayTime;
                view_locate_info.space = m_play_space;
    
                uint32_t view_capacity_input = (uint32_t)m_views.size();
                uint32_t view_count_output = 0; // To check if the count matches
    
                result = xrLocateViews(m_session, &view_locate_info, &view_state,
                                       view_capacity_input, &view_count_output, m_views.data());
    
                if (!xr_check(m_instance, result, "xrLocateViews() failed!")) {
                    // If view location fails, we probably can't render this frame.
                    // We still need to call xrEndFrame, likely with no layers.
                    frame_state.shouldRender = false; // Force skip rendering
                } else if (view_count_output != view_capacity_input) {
                     printf("Warning: xrLocateViews returned %u views, expected %u\n", view_count_output, view_capacity_input);
                     // Handle mismatch? For now, proceed cautiously or skip rendering.
                     frame_state.shouldRender = false;
                } else if (!(view_state.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) ||
                           !(view_state.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT)) {
                    // Views located, but pose is not valid. Skip rendering.
                    printf("xrLocateViews reports invalid view pose. Skipping render.\n");
                    frame_state.shouldRender = false;
                }
                else {
                     located_views = true; // Views are valid and ready for rendering
                }
            }
    
    
            // --- Render Views to Swapchains ---
            bool rendered_successfully = true;
            if (frame_state.shouldRender && located_views) {
                for (uint32_t i = 0; i < m_view_count; ++i) {
                    if (!RenderView(i, frame_state)) {
                        rendered_successfully = false;
                        break; // Stop rendering if one view fails
                    }
                }
            } else {
                 // If not rendering, we still need to release any acquired swapchain images
                 // Typically done by submitting an empty EndFrame, but ensure no images are held.
                 // Good practice: Only acquire/wait/render/release inside the shouldRender block.
                 printf("Skipping render pass (shouldRender=%d, located_views=%d)\n", frame_state.shouldRender, located_views);
            }
    
    
            // --- End Frame ---
            XrCompositionLayerProjection projection_layer = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
            projection_layer.space = m_play_space;
            projection_layer.viewCount = m_view_count;
            projection_layer.views = m_projection_views.data(); // Use the pre-filled data
    
            const XrCompositionLayerBaseHeader* submitted_layers[1] = {
                 reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projection_layer)
            };
    
            XrFrameEndInfo frame_end_info = {XR_TYPE_FRAME_END_INFO};
            frame_end_info.displayTime = frame_state.predictedDisplayTime;
            frame_end_info.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE; // Or other modes
    
            // Submit layers only if rendering was intended and successful, and poses are valid.
            if (frame_state.shouldRender && rendered_successfully && located_views &&
                (view_state.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) &&
                (view_state.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT))
            {
                frame_end_info.layerCount = 1;
                frame_end_info.layers = submitted_layers;
            } else {
                // Submit no layers if we skipped rendering or views were invalid
                frame_end_info.layerCount = 0;
                frame_end_info.layers = nullptr;
                 if (!frame_state.shouldRender) printf("Submitting 0 layers because shouldRender is false.\n");
                 else if (!located_views) printf("Submitting 0 layers because views could not be located.\n");
                 else if (!rendered_successfully) printf("Submitting 0 layers because rendering failed.\n");
                 else printf("Submitting 0 layers because view pose invalid.\n");
            }
    
    
            result = xrEndFrame(m_session, &frame_end_info);
            if (!xr_check(m_instance, result, "xrEndFrame() failed!")) return false; // Critical failure
    
            return true; // Frame completed successfully
        }
    
    
        bool RenderView(uint32_t view_index, const XrFrameState& frame_state) {
            // --- Acquire Swapchain Image ---
            uint32_t acquired_index;
            XrSwapchainImageAcquireInfo acquire_info = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            XrResult result = xrAcquireSwapchainImage(m_swapchains[view_index], &acquire_info, &acquired_index);
            // If acquire fails, we might be running too fast or have a deeper issue.
            // Retrying might be an option, but for simplicity, treat as error.
            if (!xr_check(m_instance, result, "Failed to acquire swapchain image for view %d", view_index)) return false;
    
    
            // --- Wait for Swapchain Image ---
            XrSwapchainImageWaitInfo wait_info = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            wait_info.timeout = XR_INFINITE_DURATION; // Or a reasonable timeout (e.g., 1 second in ns)
            result = xrWaitSwapchainImage(m_swapchains[view_index], &wait_info);
            // If wait fails (e.g., timeout), something is wrong.
            if (!xr_check(m_instance, result, "Failed to wait for swapchain image for view %d", view_index)) return false;
    
            // --- Prepare Rendering ---
            // Update projection view pose and FoV from located views
            m_projection_views[view_index].pose = m_views[view_index].pose;
            m_projection_views[view_index].fov = m_views[view_index].fov;
    
            // Create projection and view matrices
            XrMatrix4x4f projection_matrix;
            XrMatrix4x4f_CreateProjectionFov(&projection_matrix, GRAPHICS_OPENGL, m_views[view_index].fov, m_near_z, m_far_z);
    
            XrMatrix4x4f view_matrix;
            XrMatrix4x4f_CreateViewMatrix(&view_matrix, m_views[view_index].pose.position, m_views[view_index].pose.orientation);
    
            int width = m_viewconfig_views[view_index].recommendedImageRectWidth;
            int height = m_viewconfig_views[view_index].recommendedImageRectHeight;
            GLuint framebuffer = m_gl_framebuffers[view_index][acquired_index];
            GLuint image_texture = m_swapchain_images[view_index][acquired_index].image; // The OpenGL texture handle
    
            // --- Execute Render Commands ---
            // Ensure GL context is current for this thread if necessary
            glXMakeCurrent(m_graphics_binding_gl.xDisplay, m_graphics_binding_gl.glxDrawable, m_graphics_binding_gl.glxContext);
    
            render_frame(width, height, m_gl_shader_program_id, m_gl_VAO,
                         frame_state.predictedDisplayTime, view_index,
                         projection_matrix, view_matrix,
                         framebuffer, image_texture);
    
            // --- Release Swapchain Image ---
            XrSwapchainImageReleaseInfo release_info = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            result = xrReleaseSwapchainImage(m_swapchains[view_index], &release_info);
            // If release fails, it's usually a critical error (e.g., wrong state)
            if (!xr_check(m_instance, result, "Failed to release swapchain image for view %d", view_index)) return false;
    
            return true;
        }
    
        // ========================================================================
        // Cleanup Methods
        // ========================================================================
    
        void CleanupRenderResources() {
             if (m_gl_shader_program_id != 0 || m_gl_VAO != 0) { // Check if initialized
                printf("Cleaning up GL render resources...\n");
                 // Ensure context is current for cleanup
                 if (m_graphics_binding_gl.glxContext) {
                     glXMakeCurrent(m_graphics_binding_gl.xDisplay, m_graphics_binding_gl.glxDrawable, m_graphics_binding_gl.glxContext);
                 }
                 // cleanup_gl needs adapting for std::vector
                 m_gl_shader_program_id = 0;
                 m_gl_VAO = 0;
                 m_gl_framebuffers.clear();
             }
        }
    
        void CleanupPlatformGraphics() {
            if (m_graphics_binding_gl.xDisplay != nullptr) { // Check if initialized
                printf("Cleaning up platform graphics...\n");
                 // Zero out the relevant parts of the binding struct after cleanup
                 m_graphics_binding_gl.xDisplay = nullptr;
                 m_graphics_binding_gl.glxContext = nullptr;
                 m_graphics_binding_gl.glxDrawable = 0;
                 // Keep type initialized
                 m_graphics_binding_gl.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR;
            }
        }
    
        void CleanupSwapchains() {
            if (!m_swapchains.empty()) {
                 printf("Destroying swapchains...\n");
                for (uint32_t i = 0; i < m_view_count; ++i) {
                    if (m_swapchains[i] != XR_NULL_HANDLE) {
                        xrDestroySwapchain(m_swapchains[i]);
                    }
                }
                m_swapchains.clear();
                m_swapchain_images.clear(); // Vectors manage their own memory
                m_swapchain_lengths.clear();
            }
        }
    };
    
    
    // ========================================================================
    // Main Application Entry Point
    // ========================================================================
    
    int main(int argc, char** argv) {
        OpenXRApp app;
    
        if (!app.Initialize()) {
            printf("Application initialization failed!\n");
            // Shutdown might cleanup partially initialized resources
            app.Shutdown();
            return 1;
        }
    
        app.Run();
    
        app.Shutdown();
    
        printf("Application finished cleanly.\n");
        return 0;
    }
    
