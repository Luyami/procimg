inline OperationRegistry OperationRegistry::create(
    std::string name,
    GLuint image,
    int width,
    int height,
    int workX,
    int workY,
    int workW,
    int workH,
    FilterSettings settings
){
    OperationRegistry registry;

    registry.name = name;

    registry.image  = image;
    registry.width  = width;
    registry.height = height;
    registry.workX  = workX;
    registry.workY  = workY;
    registry.workW  = workW;
    registry.workH  = workH;

    auto now = std::chrono::system_clock::now();
    OperationDate t = std::chrono::system_clock::to_time_t(now);
    registry.date = t;

    registry.settings = settings;

    return registry;
}  