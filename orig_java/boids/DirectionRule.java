/*
 * DirectionRule.java
 *
 * Created on 25 May 2007, 17:48
 *
 * Creates a FieldOfView to store one of six fields of view per bird:
 * 3 to check for PreyBoids, 3 to look for PredatorBoids
 * Also stores an ArrayList of which boids have been found in it, so that
 * a new vector can be calculated.
 * This rule's vector is then added to the rest to produce a final new position.
 */
package boids;

import java.awt.Point;
import java.awt.geom.*;
import java.util.ArrayList;

/**
 *
 * @author Olner Dan
 */
public class DirectionRule {

    /****
     * Instance variables:
     ***/
    //GeneralPath object: an eight-vertex object representing the field of view for this rule
    GeneralPath fieldOfView;
    //Used to store the numbers that encode this field of view rule
    Point2D.Double[] pathGenes = new Point2D.Double[8];
    //This is actually the length of the path of the direction
    double speed;
    //to be added to all the other ruleweights and used to apportion their importance
    double ruleweight;
    //the importance placed on how far away other detected boids are:
    //from 0 to 2
    //Multipy rule-weight by this calculation when the boid knows the distance
    double distanceImportance;
    
    double relativeAngle;
    //used to store boids that hove into the view of this rule
    
    
    //Vars for reacting to the direction and speed of detected boids
    double vectorspeed;
    //to be added to all the other ruleweights and used to apportion their importance
    double vectorruleweight;
    //the importance placed on how far away other detected boids are:
    //from 0 to 2
    double vectorrelativeAngle;
    //used to store boids that hove into the view of this rule
    
    
    
    public ArrayList<Boid> boidsInView = new ArrayList<Boid>();
    //average distance and angle to boids, calculated when this rule detects a presence
    double[] avDistanceAndAngle;
    AngleFromTwoPoints getAngle = new AngleFromTwoPoints();
    //id of owner boid
    int id;

    //the chosen radius from the GUI
    int fovRadius;
    
    /** Creates a new instance of DirectionRule */
    public DirectionRule(int s, int id, int fovRadius) {

        this.id = id;
        
        this.fovRadius = fovRadius;

        //0 = start of evolution
        //So do what needs to be done to randomise the boid's genes.
        switch (s) {

            //What to do if its the start of the boid-world: we need to begin the randomisation process
            case 0:
                randomise();
                break;

            //A baby Boid's empty rule, waiting for its parent's genes...
            case 1:
                fieldOfView = new GeneralPath();

        }


    }

    //Method for randomising all values for a new baby boid
    private void randomise() {

        //start by randomising the GeneralPath that represents its field of view
        //this works on the eight directions of a spider diagram, so each point is randomised
        //along a line between 0 and the chosen radius (so the maximum circumference is 200)
        //Its presumed to be aligned to 0 radians initially; the boid itself can turn it round
        //to be aligned with its direction.
        //I can't think of a neat way of doing this except one at a time...

        //Create a point on the GeneralPath, then rotate it by PI/4 i.e. 1/8 of a circle
        //then make the next one...
        fieldOfView = new GeneralPath();
        Point2D t;
        AffineTransform tx;

        for (int i = 0; i < 8; i++) {

            //Each vertex in this field of vision is between 0 and chosen radius value
            //but its a square rule, to make it, on average, a little smaller...
            double a = ((Utils.r.nextDouble() * Math.sqrt( (double) fovRadius)));
            a = a * a;
            //a = a/1.3;

            //System.out.println("Size of vertex: " + a);

            int sizeOfVertex = (int) a;

            if (i == 0) {
                fieldOfView.moveTo(sizeOfVertex, 0);
            } else {
                fieldOfView.lineTo(sizeOfVertex, 0);
            }

            //now rotate, ready for the next one...
            tx = new AffineTransform();
            tx.rotate((Math.PI / 4));
            fieldOfView = new GeneralPath(tx.createTransformedShape(fieldOfView));


            //Now we need to load this final shape into the rule's genes
            PathIterator geoff = fieldOfView.getPathIterator(null);

            double[] coords = new double[6];

            for (int j = 0; j < 8; j++) {

                geoff.currentSegment(coords);

                pathGenes[j] = new Point2D.Double();
                pathGenes[j].setLocation(coords[0], coords[1]);

                ////System.out.println("Segment coords=: "+coords[0] +","+coords[1]);

                geoff.next();


            }



        //t = fieldOfView.getCurrentPoint();

        // //System.out.println("Current vertex location=: "+ t.getX()+","+t.getY() );

        }

        fieldOfView.closePath();

        //Randomise position-reaction speed;
        speed = (Utils.r.nextDouble() * 10);

        //Randomise position-reaction ruleWeight;
        //ruleweight = (Utils.r.nextDouble());
        ruleweight = 0.1;

        //Randomise distanceImportance;
        distanceImportance = (Utils.r.nextDouble() * 2);

        //randomise position-reaction relativeAngle;
        relativeAngle = (Utils.r.nextDouble() * (Math.PI * 2));
        
        
        //Randomise vector-reaction speed - 
        //between -1 and 1: a reaction to the detected av speed of boids        
        vectorspeed = (Utils.r.nextDouble()*2)-1;

        //Randomise vector-reaction ruleWeight;
        //vectorruleweight = (Utils.r.nextDouble());
        vectorruleweight = 1;
        
        
        //randomise vector-reaction relativeAngle;
        vectorrelativeAngle = (Utils.r.nextDouble() * (Math.PI * 2));
        
        
        
    //relativeAngle=Math.toRadians(350);

    }

    //Note: this is a lot of work, computationally.
    //If it turns out to be too slow, we should just resort to a boolean
    // - "are there any boids of the appropriate type nearby? Yes or no?"
    // and get their average direction separately somehow.
    
    //Method for storing a list of boids that end up in this field of view's sight
    public void boidsInView(Boid newBoid) {

        boidsInView.add(newBoid);

    }

    //calculate the average distance from a point to detected boids
    //and the average (absolute) angle to their position
    //and finally the average velocities and directions of detected boids
    public double[] avPosDistAngle(Point2D.Double p) {

        double x = 0;
        double y = 0;
        double[] posDistAngle = new double[6];

        
        //If there's only one boid, we can cut the calculations down greatly...
        if (boidsInView.size() > 1) {
            
            
            ////System.out.println("BoidsinView size="+boidsInView.size() );

            //Get all the x and y..s
            //PosDistAngle: 0 and 1 are distance and angle
            //and 2 and 3 are the av x and y coords of the detected boids
            //and 4 and 5 are the average angle and velocity of the boids

            //average angle of boids requires a y and x to go into atan2
            double[] avAngleofBoids = new double[2];
            double avSpeedofBoids = 0;

            //System.out.println("Getting average direction of detected boids...");
            for (Boid z : boidsInView) {

//                if (id == 0) {
//                    System.out.println("DirRule: Boid detected at: x=" + z.xyLocation[0].x + ",y=" + z.xyLocation[0].y);
//                }

                x += z.xyLocation[0].x;
                y += z.xyLocation[0].y;

                //System.out.println("Current direction of this detected boid: " + Math.toDegrees(z.currentDirection));
                avAngleofBoids[0] += Math.cos(z.currentDirection);
                avAngleofBoids[1] += Math.sin(z.currentDirection);
                
                avSpeedofBoids += z.currentSpeed;

            }

            //Then average them...
            int tempSize = boidsInView.size();

            x = (x / tempSize);
            y = (y / tempSize);

            avAngleofBoids[0] += avAngleofBoids[0] / tempSize;
            avAngleofBoids[1] += avAngleofBoids[1] / tempSize;

            //do the atan2
            posDistAngle[4] = Math.atan2(avAngleofBoids[1], avAngleofBoids[0]);

            //If they're negative, add one whole circle
            if (posDistAngle[4] < 0) {posDistAngle[4] += Math.PI * 2;}

            //av speed...
            posDistAngle[5] = avSpeedofBoids/tempSize;
            
            //System.out.println("Final average direction of detected boids:" + Math.toDegrees(PosDistAngle[3]));

//            if (id == 0) {
//                System.out.println("DirRule: Calculated average absolute position of boids: x = " + x + ",y=" + y);
//            }

            //Now we need to work out the distance between these co-ordinates and ourselves...
            x = (p.x - x);
            y = (p.y - y);

//            if (id == 0) {
//                System.out.println("DirRule: My position: x=" + p.x + ",y=" + p.y);
//                System.out.println("DirRule: Calculated average distance to boids relative to my position: x = " + x + ",y=" + y);
//            }

            //The handy squaring in pythag will take care of any negative numbers for us...
            posDistAngle[0] = (Math.sqrt((x * x) + (y * y)));

            //Now to work out the angle, find a asin of y / distance...
            posDistAngle[1] = getAngle.getAngleFromXY(x, y);

            //Since this seems to be 180 degrees out, let's flip it:
            posDistAngle[1] += Math.PI;
            posDistAngle[1] %= (Math.PI * 2);

//            if (id == 0) {
//                System.out.println("DirRule: Calculated angle to this position:" + Math.toDegrees(posDistAngle[1]) + "\n");
//            }

            //Third, the average position...
            posDistAngle[2] = x;
            posDistAngle[3] = y;

            ////System.out.println("average distance is currently: "+PosDistAngle[0]);
            ////System.out.println("DirRule: average angle is currently: "+(Math.toDegrees(PosDistAngle[1]))) ;



            
        //else if boidsinview size is only one - 
        //that is, if there's only one boid in this field of view,
        //we can do this much quicker
        } else {
                     
            Boid b = boidsInView.get(0);
            
            //PosDistAngle: 0 and 1 are distance and angle
            //and 2 and 3 are the av x and y coords of the detected boids
            //and 4 and 5 are the average angle and velocity of the boids
            
            //But here, there's only one boid, so life is a lot easier...
            //Now we need to work out the distance between these co-ordinates and ourselves...
            x = (p.x - b.xyLocation[0].x);
            y = (p.y - b.xyLocation[0].y);

            
            //The handy squaring in pythag will take care of any negative numbers for us...
            posDistAngle[0] = (Math.sqrt((x * x) + (y * y)));

            //Now to work out the angle, find a asin of y / distance...
            posDistAngle[1] = getAngle.getAngleFromXY(x, y);

            //Since this seems to be 180 degrees out, let's flip it:
            posDistAngle[1] += Math.PI;
            posDistAngle[1] %= (Math.PI * 2);
            
            
            //Third, the average position...
            posDistAngle[2] = x;
            posDistAngle[3] = y;
            
            //lastly, the one boid's direction and speed...
            posDistAngle[4] = b.currentDirection;
            posDistAngle[5] = b.currentSpeed;
            
          

        }// end else

          return posDistAngle;
        
    }

    public boolean didISeeAny() {

        if (boidsInView.isEmpty()) {
            return false;
        } else {
            return true;
        }

    }

//to empty the ArrayList of boids we saw on this turn to make room for next turn
    public void emptyBoidsInView() {

        boidsInView.clear();

    }

    public GeneralPath getFieldOfView() {

        return fieldOfView;

    }
}
