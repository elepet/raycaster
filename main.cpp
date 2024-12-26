#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include <array>
#include <stack>
#include <queue>
#include <cmath>
#include <fstream>
#include <iostream>


//fix: fisheye, collision when reversing, button toggles, level editor, block top
//add: textures, up/down, gameplay, gamify, doors, sky, jump, angles

int main() {
    //settings
    constexpr bool testFPS=false; //if true prints max fps
    constexpr int screenW=900; //in px
    constexpr int screenH=600;
    constexpr int row=20; //number of rows in map
    constexpr int column=20; //number of columns in map
    constexpr int shadowStrength=50;
    constexpr int speed=100; //player movement speed
    constexpr int res=200; //resolution
    constexpr float coefficient=0.3;
    constexpr int fov=coefficient*res; //field of view in degrees
    constexpr float wallHeight1=2; //heights of respective wall types
    constexpr float wallHeight2=1;
    constexpr float wallHeight3=0.3;
    constexpr float playerHeight=1;
    //constexpr float tol=10; //px from wall where collision activates

    //colours
    sf::Color white(255,255,255);
    sf::Color lightGrey(240,240,240);
    sf::Color grey(180,180,180);
    sf::Color darkGrey(140,140,140);

    //map
    int map[row][column];
    int rowCounter=0;
    int columnCounter=0;
    std::ifstream mapFileIn("map.txt");
    if(!mapFileIn){throw std::runtime_error("error: open file for input failed!\n");}
    while (!mapFileIn.eof()) {
        int i;
        mapFileIn>>i;
        map[rowCounter][columnCounter]=i;
        columnCounter++;
        if (columnCounter%column==0){columnCounter=0;rowCounter++;}
    }
    mapFileIn.close();

    //grid
    int numPoints=2*row+2*column+2;
    sf::VertexArray grid(sf::Lines, numPoints);
    float gridX=0;
    float gridY=0;
    for (int i=0;i<2*column+2;i++) { //ver
        grid[i].position=sf::Vector2f(gridX,0);
        i++;
        grid[i].position=sf::Vector2f(gridX,screenH);
        gridX+=screenW/column;
    }
    for (int i=2*column+2;i<numPoints;i++) { //hor
        grid[i].position=sf::Vector2f(0,gridY);
        i++;
        grid[i].position=sf::Vector2f(screenW,gridY);
        gridY+=screenH/row;
    }
    sf::RectangleShape tile;
    tile.setSize(sf::Vector2f(screenW/column, screenH/row));
    tile.setFillColor(grey);

    //ceiling and floor
    sf::RectangleShape ceiling;
    ceiling.setSize(sf::Vector2f(screenW,screenH/2));
    ceiling.setFillColor(lightGrey);
    sf::RectangleShape floor;
    floor.setSize(sf::Vector2f(screenW,screenH/2));
    floor.setPosition(sf::Vector2f(0,screenH/2));
    floor.setFillColor(darkGrey);

    //textures
    sf::Texture texture;
    texture.loadFromFile("texture1.png");

    //player
    sf::Vector3f p(screenW/2,screenH/2,0); //position and direction (x, y, theta in rad)
    sf::ConvexShape player;
    player.setPointCount(3);
    player.setPoint(0, sf::Vector2f(p.x,p.y)); //drawing as triangle
    player.setPoint(1, sf::Vector2f(p.x-20,p.y-5.36));
    player.setPoint(2, sf::Vector2f(p.x-20,p.y+5.36));
    player.setFillColor(sf::Color::Green);
    player.setOrigin(p.x,p.y);
    int setSpeed; //used for logic

    //ray
    std::array<sf::VertexArray, res> ray;
    for (int i=0;i<res;i++) {
        ray[i]=sf::VertexArray(sf::PrimitiveType::Lines,2);
        ray[i][0].color=sf::Color::Red;
        ray[i][1].color=sf::Color::Red;
    }

    //algorithm use
    int side;
    sf::Vector2f scaledPos; //scaled position
    sf::Vector2i truncatedPos; //truncated scaled position
    sf::Vector2f internalPos; //internal position in square
    sf::Vector2f n; //normalised direction vector (max length 1)
    sf::Vector2f distX; //x: dist to first ver intercept, y: dist between consecutive ver intercepts
    sf::Vector2f distY; //x: dist to first hor intercept, y: dist between consecutive hor intercepts
    sf::Vector2f unit; //x: 1 or -1 step for ver intercept, y: 1 or -1 step for hor intercept
    float wallDist; //dist to wall from camera plane
    float interceptXPos; //pos of hor intercept along wall (0~1). used for texture
    float interceptYPos; //pos of ver intercept along wall (0~1). used for texture
    struct RayInfo { //info on each hit of each ray (multiple for dif square types)
        int id; //hit type
        int side;
        float distX; //distX to hit
        float distY; //distY to hit
        float interceptXPos; //hor position of hit 0~1 on face
        float interceptYPos; //ver position of hit 0~1 on face
    };
    std::stack<RayInfo> renderStack; //remembers order of hits to then render in reverse

    //misc
    sf::RenderWindow window(sf::VideoMode(screenW, screenH), "Ray-Caster", sf::Style::Titlebar | sf::Style::Close);
    sf::Clock clock;
    bool menu=false;
    std::queue<sf::RectangleShape> wall;
    float wallHeight;

    while (window.isOpen()) {
        //fps and time setup
        if (testFPS==false)window.setFramerateLimit(60);
        sf::Time dt=clock.restart(); //delta time

        //keyboard commands
        sf::Event event;
        while (window.pollEvent(event)) {
            //close
            if (event.type==sf::Event::Closed) window.close();
            if (event.type==sf::Event::KeyPressed&&event.key.code==sf::Keyboard::Escape) window.close();

            //menu
            if (event.type==sf::Event::KeyPressed&&event.key.code==sf::Keyboard::E) menu=true;
            if (event.type==sf::Event::KeyPressed&&event.key.code==sf::Keyboard::E&&
                sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)) menu=false;

            //level editing
            if(sf::Mouse::isButtonPressed(sf::Mouse::Left)&&
                event.type==sf::Event::KeyPressed&&
                event.key.code==sf::Keyboard::Num1&&
                menu==true) {
                map[static_cast<int>(row*sf::Mouse::getPosition(window).y/screenH)]
                [static_cast<int>(column*sf::Mouse::getPosition(window).x/screenW)]=1;
            }
            if(sf::Mouse::isButtonPressed(sf::Mouse::Left)&&
                event.type==sf::Event::KeyPressed&&
                event.key.code==sf::Keyboard::Num2&&
                menu==true) {
                map[static_cast<int>(row*sf::Mouse::getPosition(window).y/screenH)]
                [static_cast<int>(column*sf::Mouse::getPosition(window).x/screenW)]=2;
            }
            if(sf::Mouse::isButtonPressed(sf::Mouse::Left)&&
                event.type==sf::Event::KeyPressed&&
                event.key.code==sf::Keyboard::Num3&&
                menu==true) {
                map[static_cast<int>(row*sf::Mouse::getPosition(window).y/screenH)]
                [static_cast<int>(column*sf::Mouse::getPosition(window).x/screenW)]=3;
            }
            if(sf::Mouse::isButtonPressed(sf::Mouse::Left)&&
                sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)&&menu==true) {
                map[static_cast<int>(row*sf::Mouse::getPosition(window).y/screenH)]
                [static_cast<int>(column*sf::Mouse::getPosition(window).x/screenW)]=0;
            }
        }

        //keyboard control setup
        p.z=player.getRotation()*M_PI/180;
        setSpeed=speed;

        //DDA algo. checks grid+1 in direction of next nearest intercept for wall
        for (int i=0;i<res;i++) {
            n.x=cos(p.z+(-(fov/2)+i*coefficient)*M_PI/180);
            n.y=sin(p.z+(-(fov/2)+i*coefficient)*M_PI/180);
            if (n.y==0)n.y=0.00001; //avoiding division by zero

            scaledPos={column*p.x/screenW,row*p.y/screenH};
            truncatedPos={static_cast<int>(scaledPos.x),static_cast<int>(scaledPos.y)};
            internalPos={scaledPos.x-static_cast<float>(truncatedPos.x),
            scaledPos.y-static_cast<float>(truncatedPos.y)};

            if (n.x>0) {
                distX.x=(1-internalPos.x)/n.x;
                unit.x=1;
                distX.y=1/n.x;
            } else {
                distX.x=internalPos.x/n.x;
                unit.x=-1;
                distX.y=1/n.x;
            }
            if (n.y>0) {
                distY.x=(1-internalPos.y)/n.y;
                unit.y=1;
                distY.y=1/n.y;
            } else {
                distY.x=internalPos.y/n.y;
                unit.y=-1;
                distY.y=1/n.y;
            }

            //ray casting loop
            while (true) {
                if (abs(distX.x)<abs(distY.x)) { //true if distance to next ver intercept shorter
                    side=1;
                    distX.x+=distX.y;
                    truncatedPos.x+=unit.x;
                } else {
                    side=0;
                    distY.x+=distY.y;
                    truncatedPos.y+=unit.y;
                }

                //finding position of intercept along wall (0~1) for texture
                interceptXPos=scaledPos.x+(abs(distY.x)-abs(distY.y))*n.x;
                interceptYPos=scaledPos.y+(abs(distX.x)-abs(distX.y))*n.y;
                if (interceptXPos>1)interceptXPos=interceptXPos-static_cast<int>(interceptXPos);
                if (interceptYPos>1)interceptYPos=interceptYPos-static_cast<int>(interceptYPos);
                if (n.x>0&&n.y>0) { //q1
                    interceptXPos=1-interceptXPos;
                } else if (n.x<0&&n.y>0) { //q2
                    interceptXPos=1-interceptXPos;
                    interceptYPos=1-interceptYPos;
                } else if (n.x<0&&n.y<0) { //q3
                    interceptYPos=1-interceptYPos;
                }


                if (map[truncatedPos.y][truncatedPos.x]==1) { //tallest wall/end condition
                    renderStack.push({map[truncatedPos.y][truncatedPos.x],side,distX.x,distY.x,interceptXPos,interceptYPos});
                    break;
                }
                if (map[truncatedPos.y][truncatedPos.x]!=0) { //closer intercept
                    renderStack.push({map[truncatedPos.y][truncatedPos.x],side,distX.x,distY.x,interceptXPos,interceptYPos});
                    if (abs(distX.x)<abs(distY.x)) { //further intercept
                        renderStack.push({map[truncatedPos.y][truncatedPos.x],1,distX.x+distX.y,distY.x,interceptXPos,interceptYPos});
                    } else {
                        renderStack.push({map[truncatedPos.y][truncatedPos.x],0,distX.x,distY.x+distY.y,interceptXPos,interceptYPos});
                    }
                }
            }

            //ray calculation
            ray[i][0].position={p.x,p.y};
            if (renderStack.top().side==1) {
                ray[i][1].position={p.x+n.x*screenW*abs(distX.x-distX.y)/column,
                p.y+n.y*screenH*abs(distX.x-distX.y)/row};
            } else ray[i][1].position={p.x+n.x*screenW*abs(distY.x-distY.y)/column,
                p.y+n.y*screenH*abs(distY.x-distY.y)/row};
            ray[i][0].position={p.x,p.y};

            //wall setup
            while (!renderStack.empty()) {
                renderStack.top().side==1 ? wallDist=(abs(renderStack.top().distX)-abs(distX.y)) :
                wallDist=(abs(renderStack.top().distY)-abs(distY.y));

                switch (renderStack.top().id) {
                    case 1:
                    wallHeight=wallHeight1;
                    break;
                    case 2:
                    wallHeight=wallHeight2;
                    break;
                    case 3:
                    wallHeight=wallHeight3;
                    break;
                    default:
                    wallHeight=1;
                    break;
                }
                if (renderStack.top().side==1) {
                    white.r-=(0.3*shadowStrength);
                    white.g-=(0.3*shadowStrength);
                    white.b-=(0.3*shadowStrength);
                }
                white.r-=wallDist*wallDist*shadowStrength/res;
                white.g-=wallDist*wallDist*shadowStrength/res;
                white.b-=wallDist*wallDist*shadowStrength/res;
                sf::RectangleShape rect;
                rect.setFillColor(white);
                white.r=255;white.g=255;white.b=255;
                rect.setSize(sf::Vector2f(1+screenW/res, wallHeight*screenH/wallDist));
                rect.setOrigin(screenW/column,rect.getSize().y);
                rect.setPosition((screenW/column+i*screenW/res),screenH/2+screenH/(2*wallDist));

                rect.setTexture(&texture);
                int texStartCoordX;
                renderStack.top().side==1 ? texStartCoordX=renderStack.top().interceptYPos*32 :
                texStartCoordX=renderStack.top().interceptXPos*32;
                rect.setTextureRect(sf::IntRect({texStartCoordX, 0}, {32/res, 32}));

                wall.push(rect);

                renderStack.pop();
            }
        }

        //keyboard control
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) { //forwards
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)) {setSpeed*=3;} //zoomies
            p.x+=cos(p.z)*setSpeed*dt.asSeconds(); p.y+=sin(p.z)*setSpeed*dt.asSeconds();
        } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) { //backwards
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)) {setSpeed*=3;} //zoomies
            p.x-=cos(p.z)*setSpeed*dt.asSeconds(); p.y-=sin(p.z)*setSpeed*dt.asSeconds();
        }
        player.setPosition(p.x, p.y);
        setSpeed=speed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) { //rotate ccw
            player.setRotation(player.getRotation()-1.8*setSpeed*dt.asSeconds());
        } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) { //rotate cw
            player.setRotation(player.getRotation()+1.8*setSpeed*dt.asSeconds());}

        if (menu==true) {
            window.clear(sf::Color::Black);
            for(int i=0;i<row;i++) {
                for(int j=0;j<column;j++){
                    if(map[i][j]!=0)
                    {
                        tile.setPosition(j*screenW/column,i*screenH/row);
                        window.draw(tile);
                    }
                }
            }
            window.draw(grid);
            window.draw(player);
            for (int i=0;i<res;i++) {
                window.draw(ray[i]);
            }
            window.display();
        } else {
            //rendering
            window.clear(sf::Color::Black);
            window.draw(ceiling);
            window.draw(floor);
            while (!wall.empty()) {
                window.draw(wall.front());
                wall.pop();
            }
            window.display();
            if (testFPS==true)std::cout<<(1/dt.asSeconds())<<'\n';
        }
    }
    //saving map
    std::ofstream mapFileOut("map.txt");
    if(!mapFileOut){throw std::runtime_error("error: open file for output failed!\n");}
    for(int i=0;i<row;i++) {
        for(int j=0;j<column;j++){
            mapFileOut<<map[i][j]<<'\n';
        }
    }
    mapFileOut.close();
    return 0;
}

