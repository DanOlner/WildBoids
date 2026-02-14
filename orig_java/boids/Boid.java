/*
 * Boid.java
 *
 * Created on 25 May 2007, 17:16
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */
package boids;

import java.awt.geom.AffineTransform;
import java.awt.geom.Point2D;

/**
 *
 * @author Olner Dan
 */
public abstract class Boid {

    //Instance variables of the BOIDS super-class
    //My ID number - an easy way of taking me out of the array when I die, rather than faffing about
    //finding out which one I am
    int id;
    //It needs to know where to go when it wants to evolve; boidsCage tells it this
    //when its first created. This boid can then, a ha, pass evolution on to its
    //descendent...
    Evolution evolution;
    //Keywords for the type of direction Rule being made
    //Depending on if this is a boid at the start of evolution
    //or a new-born baby
    final int INIT = 0;
    //Each DirectionRule is a class that can hold the state of all the necessary variables
    //Including the GenerathPath object that describes the field of vision for this rule.
    //As well as creating them for the start of evolution, when a new boid is born, the Evolution class will
    //take charge of the process of breeding and selection
    //direction rule 0,1 and 2 are for PreyBoids
    //direction rule 3,4 and 5 are for PredatorBoids
    DirectionRule[] dirRule;
    //Array for holding totalled new positions from dirRules;
    //can only ever be a maximum of six points
    //Points are created and added in constructor so we don't make them
    //on every check
    Point2D.Double[] totalNewPosHeading = new Point2D.Double[12];
    int av;
    //the speed the boid's going from the last turn - used to calculate next speed
    //taking into account speedEnertia and all the directionRules.
    double currentSpeed;
    //the direction in radians of the current heading
    //All field of view polygons should be rotated to match this.
    double currentDirection;
    double lastDirection;
    //Both of these are between 0 and 1, and control the enertia of movement
    double directionMomentum;
    double speedInertia;
    //A boid's age is a factor in determining its fitness for breeding
    //The older it is, the longer it's survived
    int age = 0;
    //Predators have metabolism that drops constantly and
    //goes up when they eat. If 0 they die.
    int metabolism;
    public int digestion = 0;
    //The current position of the boid, as well as eight virtual points
    //so that boids' fields of view can take them into account in all
    //positions of the torus: see below for explanation
    Point2D.Double[] xyLocation;
    Point2D.Double lastXYLocation = new Point2D.Double(0, 0);
    //How long have they been dead?
    //Counts down to zero; zero = remove their deathmark from screen...
    public int deadFor = 20;
    int[] cageXY;
    int[] fovSize;
    //Have any of the viewing rules found a boid?
    private boolean foundBoid;
    private AffineTransform tx;
    private AngleFromTwoPoints getAngle = new AngleFromTwoPoints();
    //Test variable to see if there's any bias in the detection of the position of boids
    //array that holds a count of the four quarters of the compass
    int[] avBoidDetectionDir = new int[4];

    /** Creates a new instance of Boid */
    //The default option: if boid() is instantiated, Boid assumes that
    //its the start of time and makes a random-gened boid
    public Boid(int id, int[] cageXY, Evolution evolution, int turningSpeed, int[] fovSize) {


        this.evolution = evolution;
        this.cageXY = cageXY;
        this.id = id;
        this.fovSize = fovSize;

        //Initialise the xyLocation array - 1 real point and 8 virtual points
        xyLocation = new Point2D.Double[9];
        for (int i = 0; i < 9; i++) {
            xyLocation[i] = new Point2D.Double(0, 0);
        }

        //set random location, using the set size of the BoidCage
        double x = (Utils.r.nextDouble() * cageXY[0]);
        double y = (Utils.r.nextDouble() * cageXY[1]);
        xyLocation[0].setLocation((int) x, (int) y);

        //System.out.println("xyloc=" + xyLocation[8].x);

        //and create the virtual points to go with it...
        setVirtualPoints();


        //This is when the boids are first made at the dawn of boid time -
        //So this little boidy needs get the DirectionRule object to
        //return randomised versions of its new genes

        //There are three set sizes for the dirRules
        //They're the same for prey and predator detection - 
        //That is, 1 & 4, 2 & 5, 3 & 6 are the same size

        dirRule = new DirectionRule[6];

        dirRule[0] = new DirectionRule(INIT, id, fovSize[0]);
        dirRule[1] = new DirectionRule(INIT, id, fovSize[1]);
        dirRule[2] = new DirectionRule(INIT, id, fovSize[2]);
        dirRule[3] = new DirectionRule(INIT, id, fovSize[0]);
        dirRule[4] = new DirectionRule(INIT, id, fovSize[1]);
        dirRule[5] = new DirectionRule(INIT, id, fovSize[2]);



        //set random currentDirection
        currentDirection = (Utils.r.nextDouble() * (Math.PI * 2));
        //currentDirection=Math.toRadians(230);

        //currentDirection=(0);


        //then translate and rotate all directionRules appropriately (all rotation of these
        //should be done within the boid class...

        for (int i = 0; i < dirRule.length; i++) {

            tx = new AffineTransform();
            tx.rotate(currentDirection);

            dirRule[i].fieldOfView.transform(tx);

            //Translate it to the boid's position...
            tx = new AffineTransform();
            tx.translate(xyLocation[0].x, xyLocation[0].y);
            dirRule[i].fieldOfView.transform(tx);



        }


        //set random current speed
        //+1 so there's no chance of being stationary...
        currentSpeed = (Utils.r.nextDouble() * 10) + 2;

        //set random inertia, both 0 to 1
        directionMomentum = (Utils.r.nextDouble() / turningSpeed);

        //test for absolute direction momentum...
        //directionMomentum = (Utils.r.nextDouble()*(Math.PI/25)); // i.e. random between 0 and 45 degrees

        //directionMomentum = 0.25;
        speedInertia = (Utils.r.nextDouble());

        //randomise metabolism
        double temp = Utils.r.nextDouble();
        temp *= 300;
        metabolism = 400 + ((int) temp);

        if (this instanceof PredatorBoid) {
            System.out.println("My birth metabolism :" + metabolism);
        }


        //Initialise Points that will be used to calculate a new heading from up to six direction rules
        for (int i = 0; i < totalNewPosHeading.length; i++) {

            totalNewPosHeading[i] = new Point2D.Double();


        }




    }// end of first constructor
    //If its a new-born, pass a reference to this new little baby boid to the
    //evolution algorithm (it will already have been passed a reference to the whole boids array.)
    //the id is the id of the old boid, which it will take - and a way of telling the boid to use this constructor
    public Boid(boolean babyBoid, int id, int[] cageXY, Evolution evolution, int[] fovSize) {

        this.cageXY = cageXY;
        this.evolution = evolution;
        this.id = id;
        this.fovSize = fovSize;

        //set random location, using the set size of the BoidCage
        xyLocation = new Point2D.Double[9];
        for (int i = 0; i < 9; i++) {
            xyLocation[i] = new Point2D.Double(0, 0);
        }

        double x = (Utils.r.nextDouble() * cageXY[0]);
        double y = (Utils.r.nextDouble() * cageXY[1]);
        xyLocation[0].setLocation((int) x, (int) y);

        //and create the virtual points to go with it...
        setVirtualPoints();



        //Come back to here to work out what else need init'ing for new-born boids
        //set random current speed
        //+1 so there's no chance of being stationary...
        currentSpeed = (Utils.r.nextDouble() * 10) + 2;

        //set random currentDirection
        currentDirection = (Utils.r.nextDouble() * (Math.PI * 2));
        //currentDirection=Math.toRadians(350);


        //randomise metabolism
        double temp = Utils.r.nextDouble();
        temp *= 300;
        metabolism = 400 + ((int) temp);

        if (this instanceof PredatorBoid) {
            System.out.println("My birth metabolism :" + metabolism);
        }


        //Initialise Points that will be used to calculate a new heading from up to six direction rules
        for (int i = 0; i < totalNewPosHeading.length; i++) {

            totalNewPosHeading[i] = new Point2D.Double();


        }


        //Make initial directionRules - the generalpath won't be set here, though
        dirRule = new DirectionRule[6];

        dirRule[0] = new DirectionRule(INIT, id, fovSize[0]);
        dirRule[1] = new DirectionRule(INIT, id, fovSize[1]);
        dirRule[2] = new DirectionRule(INIT, id, fovSize[2]);
        dirRule[3] = new DirectionRule(INIT, id, fovSize[0]);
        dirRule[4] = new DirectionRule(INIT, id, fovSize[1]);
        dirRule[5] = new DirectionRule(INIT, id, fovSize[2]);

    }

    //A separate method because this will have to happen every time a boid moves...
    private void setVirtualPoints() {

        //Create the virtual points. How this works: imagine the 'real space' is at the
        //centre, and is surrounded on each side by eight 'virtual spaces.
        //Now picture what happens when a boid is near the far right side:
        //One virtual copy is now just before the left side (because it's x - width-of-world,y)
        //And so when a boid checks one of its fields of view it will find a virtual copy
        //of this boid.  The least faffy way of doing this I could think of.
        //[0]=real point; [1] to [8] start top-left and rotate round the virtual space
        //such that [8] is to the left of real space

        //top left
        xyLocation[1].setLocation(xyLocation[0].x - cageXY[0], xyLocation[0].y - cageXY[1]);
        //top middle
        xyLocation[2].setLocation(xyLocation[0].x, xyLocation[0].y - cageXY[1]);
        //top right
        xyLocation[3].setLocation(xyLocation[0].x + cageXY[0], xyLocation[0].y - cageXY[1]);
        //right
        xyLocation[4].setLocation(xyLocation[0].x + cageXY[0], xyLocation[0].y);
        //bottom right
        xyLocation[5].setLocation(xyLocation[0].x + cageXY[0], xyLocation[0].y + cageXY[1]);
        //bottom middle
        xyLocation[6].setLocation(xyLocation[0].x, xyLocation[0].y + cageXY[1]);
        //bottom left
        xyLocation[7].setLocation(xyLocation[0].x - cageXY[0], xyLocation[0].y + cageXY[1]);
        //left
        xyLocation[8].setLocation(xyLocation[0].x - cageXY[0], xyLocation[0].y);



    }

    //to store if any of the six rules detected a boid...
    public void foundBoid() {

        foundBoid = true;

    }

    //for turning off the above rule;
    public void doneFoundBoid() {

        foundBoid = false;

    }

    //to return whether any boids found this go...
    public boolean didIFindBoid() {

        return foundBoid;

    }

    //If no boids were seen, I just carry on in the same direction...
    public void repeatPreviousHeading() {

        double x = (Math.cos(currentDirection) * currentSpeed);
        double y = (Math.sin(currentDirection) * currentSpeed);

        //xyLocation[0].translate((int)x,(int)y);

        double oldx = xyLocation[0].getX();
        double oldy = xyLocation[0].getY();

        x = (oldx + x);
        y = (oldy + y);

        lastXYLocation.setLocation(xyLocation[0]);

        lastDirection = currentDirection;

        xyLocation[0].setLocation(x, y);

        //System.out.println("xyloc - lastxyloc: "+ (xyLocation[0].x - lastXYLocation.x));

        //check for wrap-around...
        torus();

        //and make virtual points to match
        setVirtualPoints();

        moveFieldsOfView();

    }

    //If this boid's seen another boid in any of its six fields of vision, it has to work out where to move to
    //That's this rule's job to start off...
    public void setNewHeading() {

        //double av=0;
        double newAngleTemp = 0;

        //First make sure the totalNewHeading array's points are set to zero
        for (Point2D.Double p : totalNewPosHeading) {

            p.setLocation(0, 0);

        }

        //Ask each rule in turn if they want to offer a new heading
        //and add that as a point to the totalNewHeading point array
        //(We can't add them directly, because each rule has a weight
        //and we need to make sure they all add up to 1.)

        //Test var for limiting totalNewHeading to one variable only...
        //boolean totalNewHeadingTest = true;

        
        double totalWeights = 0;
        
        for (int i = 0; i < 6; i++) {

            if (dirRule[i].didISeeAny()) {
                
                
                //total our rule weights up here and store them locally to save processing time later...
                totalWeights += dirRule[i].ruleweight;

                totalWeights += dirRule[i].vectorruleweight;

                //This array has four slots: 0 = average distance, 1 = average angle between
                //the submitted point and the rule's detected boids
                //2 and 3 are the average coordinates of the detected boids...
                double[] boidPosData = dirRule[i].avPosDistAngle(xyLocation[0]);
                
//                for (double d : boidPosData) {
//
//                    System.out.println("boidPosdata: " + d);
//
//                }

                //Now calculate a new angle: that's angle to detected boids + relative angle of this rule

//                if (id == 0) {
//                    System.out.println("newAngleArray[1] is currently: " + (Math.toDegrees(boidPosData[1])));
//                    System.out.println("this rule's relative angle is: " + Math.toDegrees(dirRule[i].relativeAngle));
//                }

                //add relative angle for position reaction
                double newPosAngle = boidPosData[1] + dirRule[i].relativeAngle;
//                System.out.println("boidPosData 1: " + boidPosData[1]);
//                System.out.println("dirRule[i].relativeangle: " + dirRule[i].relativeAngle);
//                System.out.println("newPosAngle: " + newPosAngle);

                //add relative angle for vector reaction
                double newVectorAngle = boidPosData[4] + dirRule[i].vectorrelativeAngle;

                //test var to see if this angle works successfully to direct the boid, when it's the only one...
                newAngleTemp = newPosAngle;

//                if (id == 0) {
//                    System.out.println("Pos: Derived angle before modulus is: " + Math.toDegrees(newPosAngle));
//                }
                newPosAngle = newPosAngle % (Math.PI * 2);
//                if (id == 0) {
//                    System.out.println("Pos: Derived angle after modulus is: " + Math.toDegrees(newPosAngle));
//                }

//                if (id == 0) {
//                    System.out.println("Vec: Derived angle before modulus is: " + Math.toDegrees(newVectorAngle));
//                }
                newVectorAngle = newVectorAngle % (Math.PI * 2);
//                if (id == 0) {
//                    System.out.println("Vec: Derived angle after modulus is: " + Math.toDegrees(newVectorAngle));
//                }

                //Now calculate a new point based on this angle and the rule's ideal speed
                double posx = (Math.cos(newPosAngle) * (dirRule[i].speed));
                double posy = (Math.sin(newPosAngle) * (dirRule[i].speed));

                //Do the same for vector-reaction rule - for this one, reaction to
                //detected speed of boids
                double vecx = (Math.cos(newVectorAngle) * (boidPosData[5] * (dirRule[i].vectorspeed)));
                double vecy = (Math.sin(newVectorAngle) * (boidPosData[5] * (dirRule[i].vectorspeed)));

//                System.out.println("vecx vecy: " + vecx + "," + vecy);
//                System.out.println("posx posy: " + posx + "," + posy);

                //System.out.println("dirrule vectorspeed: "+ dirRule[i].vectorspeed);

//                if (id == 0) {
//                    System.out.println("The rule speed is " + dirRule[i].speed);
//                    System.out.println("And the new point that's been calculated based on this is: x=" + posx + ",y=" + posy + "\n");
//                }


                //and add these to totalNewHeading...
                totalNewPosHeading[(i * 2)].setLocation(posx, posy);
                totalNewPosHeading[(i * 2) + 1].setLocation(vecx, vecy);

            //totalNewHeadingTest = false;

            //Test: let's fix boid zero's totalnewheading, see what happens.
            //if (id == 0) {totalNewHeading[i].setLocation(5,5);}



            //test to see if there's a bias in boid detection position
                /*if (id==0) {
            if ( (newAngleArray[1]>0) && (newAngleArray[1]<(Math.PI/2))) {avBoidDetectionDir[0]++;} else
            if (newAngleArray[1]>(Math.PI/2) && newAngleArray[1]<(Math.PI)) {avBoidDetectionDir[1]++;} else
            if (newAngleArray[1]>(Math.PI) && newAngleArray[1]<((Math.PI/2)*3)) {avBoidDetectionDir[2]++;} else
            if (newAngleArray[1]>((Math.PI)/2)*3 && newAngleArray[1]<((Math.PI*2))) {avBoidDetectionDir[3]++;}
            }*/

            }//end of if statement for totalNewHeading test...


        }//end of dirRule for loop (for loading dirRule headings into totalNewHeading

        //Test there's only one value in totalNewHeading'

//        if (id == 0) {
//            for (Point2D.Double p : totalNewPosHeading) {
//
//                System.out.println("totalNewHeading value: x=" + p.x + ",y=" + p.y);
//
//            }
//            System.out.println("\n");
//        }



        /*if (id==0) {System.out.println("\n");
        for (double av : avBoidDetectionDir) {
        System.out.println("From 0 to 3: " + av);
        } }*/



        //test totalnewheading
        /*for (Point2D p : totalNewHeading) {
        System.out.println("totalNewHeading: x="+p.getX() +",y="+p.getY() );
        }*/



        //Now add them all together, adjusting for weights. Add all weights together
        //Then divide each by the total to get a fraction...
                     
        
        
//        for (int i = 0; i < 6; i++) {
//
//            if (dirRule[i].didISeeAny()) {
//
//                totalWeights += dirRule[i].ruleweight;
//
//                totalWeights += dirRule[i].vectorruleweight;
//
//            }
//
//        }

        //System.out.println("total weights = :" + totalWeights);
        
        double posx, posy, vecx, vecy = 0;
        
        //Then multiply the appropriate NewHeading by it...
        for (int i = 0; i < 6; i++) {


            if (dirRule[i].didISeeAny()) {

                //av++;

                 posx = totalNewPosHeading[i*2].getX();
                 posy = totalNewPosHeading[i*2].getY();
                
                 vecx = totalNewPosHeading[(i*2)+1].getX();
                 vecy = totalNewPosHeading[(i*2)+1].getY();
                
                //System.out.println("New heading x and y before adjustment:" + x + "," + y);

                //System.out.println("rule Weight: "+dirRule[i].ruleweight);
                //System.out.println("total weights: "+totalWeights);

                //System.out.println("ruleWeight divided by total weights: "+dirRule[i].ruleweight/totalWeights);

               // if (dirRule[i].ruleweight > 0) {

                    totalNewPosHeading[i * 2].setLocation(posx * (dirRule[i].ruleweight / totalWeights), posy * (dirRule[i].ruleweight / totalWeights));

                //}

               // if (dirRule[i].vectorruleweight > 0) {
                
                    totalNewPosHeading[(i * 2) + 1].setLocation(vecx * (dirRule[i].vectorruleweight / totalWeights), vecy * (dirRule[i].vectorruleweight / totalWeights));

              //  }  
                    
            //System.out.println("New heading x and y after adjustment:" + totalNewHeading[i].x + "," + totalNewHeading[i].y);
            //System.out.println("Rule Weight = "+dirRule[i].ruleweight);

            }


        }

        /*test getting rid of all but one new heading
        for (int i=0;i<6;i++) {
        //but we need to keep one to stop NaN problems...
        boolean keepone = false;
        if (keepone) {totalNewHeading[i].setLocation(0,0);}
        if (totalNewHeading[i].x!=0) {keepone = true;}
        }*/

        //NOW... finally, we can total the buggers up...
        
        double finalx = 0;
        double finaly = 0;

        for (Point2D.Double p : totalNewPosHeading) {

            finalx += p.x;
            finaly += p.y;

        }
        
        //reduce for momentum as well...
        finalx *= speedInertia;
        finaly *= speedInertia;
        

//        if (id == 0) {
//            System.out.println("Final relative position: x=" + finalx + ",y=" + finaly);
//        }


        //just testing: I'm going to reduce the overall speed, since it seems a little high...

//        finalx /= 2;
//        finaly /= 2;

        //Though I'm just going to try a different approach, which is to average the points in the same way we do
        //When detecting the position of boids in a field.

        //Then average them.  Though note that because totalNewHeading, in my original version, was not an ArrayList,
        //We have to work out the number to divide by.  I got that above when running through the dir Rules to see
        //Which ones had detected something.
        //finalx = (finalx/av);
        //finaly = (finaly/av);

//        if (id == 0) {
//            System.out.println("final x:" + finalx + " and final y:" + finaly);
//        }


        //And for the final, final bit, we adjust the position based on momentum. Sadly, this means
        //deriving the angle again, since what we have is a point only...
        //To work out the angle, find a atan of y / x and so some changes based on polarity of x and y
        //in our special AngleFromTwoPoints class...
        double newAngle = getAngle.getAngleFromXY(finalx, finaly);

//        if (id == 0) {
//            //No it shouldn't! newAngle is my desired angle.
//            //System.out.println("New angle and angle to detected boids should be the same:"+newAngle+","+newAngleTemp);
//
//            System.out.println("(In Boids class) Final new Angle from these is: " + Math.toDegrees(newAngle));
//        }


        //The total amount of distance we wanted to shift is just Pythag...
        double newSpeed = (Math.sqrt((finalx * finalx) + (finaly * finaly)));
        //System.out.println("New speed=:" + newSpeed);

        //Now get the final angle, which I've called momentumAngle
        double momentumAngle;
        double transformAngle;
        double comparisonNumber;

        /*1. Find: transform angle. (This will be the angle to add or subtract from current direction)
        transform angle = new-old.
        transform angle = +transform angle.
        If it's more than 180 degrees, make it the compass-opposite. (because that's the direction the boid should go in.)
         * it by momentum = final transform angle.
        2. Work out whether to add or subtract it from current angle.
        if new-old < 0 (NOT transform angle) then comparison number = -180, else its 180.
        if new-old > comparison number THEN go anti-clockwise i.e. currentDirection - final transform angle
        if new-old < comparison number THEN go clockwise i.e. currentDirection + final transform angle
        3. Check for rounding:
        if final angle < 0 then add 360 degrees ELSE if final angle > 360 then subtract 360.*/

        transformAngle = newAngle - currentDirection;

        //make it positive!
        transformAngle = Math.abs(transformAngle);

        if (transformAngle > Math.PI) {
            transformAngle = (Math.PI * 2) - transformAngle;
        }
        transformAngle *= directionMomentum;

        if (newAngle - currentDirection < 0) {
            comparisonNumber = -Math.PI;
        } else {
            comparisonNumber = Math.PI;
        }

        //if ( (newAngle-currentDirection) < comparisonNumber) {momentumAngle = currentDirection + transformAngle;}
        //else {momentumAngle = currentDirection - transformAngle;}

        if ((newAngle - currentDirection) < comparisonNumber) {
            momentumAngle = currentDirection + transformAngle;
        } else {
            momentumAngle = currentDirection - transformAngle;
        }


        if (momentumAngle < 0) {
            momentumAngle += (Math.PI * 2);
        } else if (momentumAngle > (Math.PI * 2)) {
            momentumAngle -= (Math.PI * 2);
        }


//        if (id == 0) {
//            System.out.println("\nOld direction: " + Math.toDegrees(currentDirection));
//            System.out.println("Ideal new angle: " + Math.toDegrees(newAngle));
//            System.out.println("Momentum rule: " + directionMomentum);
//            System.out.println("Calculated final new angle: " + Math.toDegrees(momentumAngle));
//            System.out.println("distance to travel:" + newSpeed + "\n");
//        }


        //We have our heading, and our hypotenuese, so our final new position is:
        finalx = (Math.cos(momentumAngle) * newSpeed);
        finaly = (Math.sin(momentumAngle) * newSpeed);
        //System.out.println("And the final coordinates given are: x="+finalx+",y="+finaly+"\n");


        //Yay!!! Now to load em in...

        double oldx = xyLocation[0].getX();
        double oldy = xyLocation[0].getY();

        //System.out.println("Old position: x="+oldx+",y="+oldy);

        finalx = (oldx + finalx);
        finaly = (oldy + finaly);

        //System.out.println("New position: x="+finalx+",y="+finaly+"\n\n");

        lastXYLocation.setLocation(xyLocation[0]);
        lastDirection = currentDirection;
        currentDirection = momentumAngle;

        xyLocation[0].setLocation(finalx, finaly);

        //System.out.println("My old position was "+lastXYLocation.x+","+lastXYLocation.y);
        //System.out.println("My new position is "+xyLocation[0].x+","+xyLocation[0].y);


        //check for wrap-around...
        torus();

        //and make virtual points to match
        setVirtualPoints();

        moveFieldsOfView();

    }

//Re-calculating co-ordinates when a boid flies off the edge of the cage...
    private void torus() {

        double x = xyLocation[0].x;
        double y = xyLocation[0].y;

        if (x < 0) {
            xyLocation[0].setLocation(x + cageXY[0], y);
        }
        if (x > cageXY[0]) {
            xyLocation[0].setLocation(x - cageXY[0], y);
        }
        if (y < 0) {
            xyLocation[0].setLocation(x, y + cageXY[1]);
        }
        if (y > cageXY[1]) {
            xyLocation[0].setLocation(x, y - cageXY[1]);
        }

    }

//translate and rotate all fields of view to keep up with the boid
//It has access to the 'current' heading, so it takes in a new one
//as well as a pair of new coordinates
    private void moveFieldsOfView() {


        for (int i = 0; i < dirRule.length; i++) {

            //Rotate: current direction
            //PI/6 = 30 degress
            //pi/2 = 90 degress
            //PI = 180 degrees
            //2PI = 360 degrees

            //calculate difference:
            //initial angle - new angle
            //if value > 0 then that's yer answer
            //if value <0 then add a full circle as well.

            //should equal zero if the boid carried along the same path
            double transformAngle = (currentDirection - lastDirection);
            //System.out.println("New boid angle =: " + transformAngle);

            //if (transformAngle!=0) {

            if (transformAngle < 0) {

                transformAngle = (transformAngle + (2 * Math.PI));

            }

            //System.out.println("Transform angle =:"+Math.toDegrees(transformAngle));

            tx = new AffineTransform();
            //Last xy because we haven't moved them yet - that happens next
            tx.rotate(transformAngle, lastXYLocation.x, lastXYLocation.y);

            dirRule[i].fieldOfView.transform(tx);

            // }

            //Translate it to the boid's position...
            //new position minus old position
            tx = new AffineTransform();
            tx.translate(xyLocation[0].x - lastXYLocation.x, xyLocation[0].y - lastXYLocation.y);
            dirRule[i].fieldOfView.transform(tx);


        }


    }

//things to do before going on to the next movement
//Some of this can be over-ridden by other boids
    public void nextMoment() {

        age++;

    }

//If and when this boid cops it, this will mark how long for
// - solely for the purpose of marking the spot where they died
//for a respectable amount of time...
    public int beenDeadFor() {

        deadFor--;
        return deadFor;

    }

//sub-classes must have a method to find out what they are
    public abstract boolean isPredator();

//Prey-specific methods to implement
//1. Predator boids have to 'register their interest' to eat a prey-boid.
//This is in case more than one has it in range at the same time.
//The prey-boid can then choose who gets to eat it. That's sick, I know.
    public abstract void iAmDinnerForWho(Boid b);

//This is for PredatorBoids to overwrite to see if a preyboid is near enough to scoff...
    public abstract void distanceToScoff();

//What predators do if they've managed to get hold of some prey...
    public abstract void eat();
}
