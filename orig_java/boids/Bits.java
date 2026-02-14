/*
 * Bits.java
 *
 * Created on 25 May 2007, 21:54
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package boids;

import java.awt.Point;
import java.awt.geom.AffineTransform;
import java.awt.geom.GeneralPath;
import java.awt.geom.Point2D;

/**
 *
 * @author Olner Dan
 */
public class Bits {
    
    /** Creates a new instance of Bits */
    public Bits() {
        
        
        //loops from boidscage for testing the paint method
        /*while (true) {
            
            for (int j=0; j<200; j++) {
                
                i++;
                
                System.out.println("Forloop = "+j);
                
                panel.repaint();
                
                try {
                    Thread.sleep(2);} catch(Exception ex) {}
            }
            
            for (int j=200; j>0; j--) {
                
                i--;
                
                System.out.println("Forloop = "+j);
                
                panel.repaint();
                
                try {
                    Thread.sleep(2);} catch(Exception ex) {}
            }
            
        }
        
    }
        
        
      /*  public void paintbits() {
            
//From boidscage - in Drawpanel, for testing...
// Create test point and surrounding polygon
            
            int i=0;
            
            Point pp = new Point(i+50, i+50);
            g2.fillOval(i,i,20,20);
            
            GeneralPath polygon = new GeneralPath(GeneralPath.WIND_EVEN_ODD);
            polygon.moveTo(i-50, i-50);
            polygon.lineTo(i+50, i-44);
            polygon.lineTo(i+50, i+10);
            polygon.lineTo(i-50, i+40);
            polygon.closePath();
            g2.draw(polygon);
            
            Point2D p = (polygon.getCurrentPoint());
            
            System.out.println("Points before:"+p.getX()+","+p.getY());
            
            AffineTransform tx = new AffineTransform();
            
            double picalc=(((Math.PI*2)*(i))/200);
            
            tx.rotate((picalc), i,i);
            
            System.out.println("PI calc = "+picalc);
            
            GeneralPath newShape = new GeneralPath(tx.createTransformedShape(polygon));
            
            p.setLocation(newShape.getCurrentPoint());
            
            System.out.println("Points after:"+p.getX()+","+p.getY());
            
            g2.draw(newShape);
            
            
        }
        
        //Test of rotation method - to be tested in paint method...
        
        public void paint(Graphics g) {
            
            
            Graphics2D g2 = (Graphics2D)g;
            
            super.paint(g2);
            
                   
            testPath = new GeneralPath();
            
            //Should be a triangle pointing east at 0 radians
            testPath.moveTo(100,100);
            testPath.lineTo(100,150);
            testPath.lineTo(200,125);
            testPath.closePath();
            
            boidAngle = (Math.PI);
            System.out.println("Initial boidangle:"+boidAngle);
            
            AffineTransform tx = new AffineTransform();
            
            tx.rotate((boidAngle), 140,125);
            
            testPath = new GeneralPath(tx.createTransformedShape(testPath));
            
            System.out.println("Angle2:"+boidAngle);
            
            g2.draw(testPath);
            
            //panel.repaint();
            
            
            //PI/6 = 30 degress
            //pi/2 = 90 degress
            //PI = 180 degrees
            //2PI = 360 degrees
            double detectedAngle = (Math.PI/2);
            
            System.out.println("Detected angle:"+detectedAngle);
            
            //calculate difference:
            //initial angle - new angle
            //if value > 0 then that's yer answer
            //if value <0 then add a full circle as well.
            
            boidAngle = (detectedAngle - boidAngle);
            
            if (boidAngle<0) {
                
                boidAngle = (boidAngle+(2*Math.PI));
                
            }
            
            System.out.println("Final angle:"+boidAngle);
            
            tx = new AffineTransform();
            
            tx.rotate((boidAngle), 140,125);
            
            testPath = new GeneralPath(tx.createTransformedShape(testPath));
            
            System.out.println("Angle2:"+boidAngle);
            
            Stroke stroke = new BasicStroke(8,
                    BasicStroke.CAP_BUTT, BasicStroke.JOIN_BEVEL, 0,
                    new float[] { 12, 12 }, 0);
            g2.setStroke(stroke);
            
            g2.draw(testPath);
            */
        
        
       /* For testing that GeneralPaths have been created properly
        
        
        GeneralPath bob;
            
            AffineTransform tx;
            
            
            for (int i=0;i<boids.length;i++) {
                
                tx = new AffineTransform();
                
                tx.translate(boids[i].xyLocation.x,boids[i].xyLocation.y);
                
                bob = boids[i].dirRuleOwnKind1.getFieldOfView();
                
                bob = new GeneralPath(tx.createTransformedShape(bob));
                
                g2.draw(bob);
                
                g2.fillOval(boids[i].xyLocation.x,boids[i].xyLocation.y,2,2);
                
            }*/
        
        
        /*Method for getting the location of points of a field of view
         *Note: in order to use it, I'll have to rotate it back to 0 again
         *
         *
             PathIterator geoff = boids[19].dirRuleOtherKind3.getFieldOfView().getPathIterator(null);
         
            double coords[];
         
        coords = new double[6];
         
        for (int i=0;i<8;i++) {
         
            geoff.currentSegment(coords);
         
            System.out.println("Segment coords=: "+coords[0] +","+coords[1]);
         
            geoff.next();

        }
         *
         *
        */
        
        /* For testing if boid detection and average distance measurement is working
         
        //cycle through each boid
        for (int i=0;i<boids.length;i++) {
            
            //cycle through each of the boid's 6 fields of vision
            for (int j=0;j<6;j++) {
                
                if (boids[i].dirRule[j].didISeeAny() ) 
                {System.out.println("I saw one and av dist=: " + boids[i].dirRule[j].avDistanceToBoids( boids[i].xyLocation[0]));}
                
            }
            
            
        }*/
        
        /*
         *
         *test trig
                
                double speed = 100;
                double direction = 20;
                
                double x = 50; double y = 50;
                
                //work out new position
                
                direction = (Math.toRadians(direction));
                
                double x2 = (Math.cos(direction)*speed);
                double y2 = (Math.sin(direction)*speed);
                
                //the original point
                g2.fillOval((int)x-5,(int)y-5,10,10);
                
                Line2D line = new Line2D.Double(x,y,x+x2,y+y2);
                
                g2.draw(line);*/
         
        
        /*Testing virtual point and rectangle detection
         *
         *int testnum=0;
            //test VirtualPoint detection
            for (boolean b : rectTest) {
                
                if (b) {testnum++;}
                
            }*/
        
        
        //Trig test for getting average angle of birds....
        /*double y = 9;
            double dist =  12.728;
                    
            System.out.println("Pi/4 =: "+Math.PI/4);
            System.out.println("angle should be about PI/4: " + Math.asin(y/dist));*/
     
        //momentum angle test
           //Start with our two angles...
        
        //this calculates correctly, but it doesn't draw the lines right!
        
           /* double oldAngle = 270;
            double newAngle = 89;
            double momentumAngle;
            double x=40;
            double momentum = 0.5;
            
            oldAngle = Math.toRadians(oldAngle);
            newAngle = Math.toRadians(newAngle);
            
           //create a line for the oldAngle; assume x=40 in both cases
            
            Line2D.Double oldLine = new Line2D.Double(50,50,50+x, 50+(Math.tan(oldAngle)*x) );
            System.out.println("Tan of old angle times x =:" + (Math.tan(oldAngle)*x) );
                        
            Line2D.Double newLine = new Line2D.Double(50,50,50+x, 50+(Math.tan(newAngle)*x) );
            System.out.println("Tan of new angle times x =:" + (Math.tan(newAngle)*x) );
            
            //Now work out the momentum angle
            momentumAngle = (((newAngle-oldAngle)*momentum) + oldAngle);
            
            if (momentumAngle>Math.PI) {momentumAngle+=Math.PI;}
            
            System.out.println("But new momentum angle calc = :" + (Math.toDegrees(momentumAngle)));
            
            //work out a line based on this
            Line2D.Double momLine = new Line2D.Double(50,50,50+x, 50+(Math.tan(momentumAngle)*x) );
            
            //plot em
            g2.setColor(Color.GREEN);
            g2.draw(oldLine);
            
            g2.setColor(Color.RED);
            g2.draw(newLine);
            
            g2.setColor(Color.BLUE);
            g2.draw(momLine);*/
            
        
        /*some test calculations
            
        
        
            Point2D.Double me = new Point2D.Double(0,0);
            Point2D.Double you = new Point2D.Double(10,-10);
            
            AngleFromTwoPoints getAngle = new AngleFromTwoPoints();
            
            double angle = getAngle.angleFromTwoPoints(me,you);
            
            //System.out.println("raw difference : x=" + x+",y="+y);
            //System.out.println("Distance between me and you: " + distance);
            System.out.println("Angle between me and you: " + angle);
            System.out.println("In degrees: " + Math.toDegrees(angle));
            
            
            /*
            double x = me.x - you.x;
            double y = me.y  - you.y;
            
            double distance = (Math.sqrt ((x*x) + (y*y)) );
            double angle = (Math.atan(y/x));
            
            System.out.println("raw difference : x=" + x+",y="+y);
            System.out.println("Distance between me and you: " + distance);
            System.out.println("Angle between me and you: " + angle);
            System.out.println("In degrees: " + Math.toDegrees(angle));
            */
            
        
        /*
         *
         
         //testing array
            
            int[] testArray = new int[10];
            
            for (int a=1; a<11; a++) {
                
                testArray[a-1] = a*2;
                
            }
            
            int[] newArray;
            
            SortArray  s = new SortArray();
            
            for (int i =0; i<testArray.length;i++) {
                
                System.out.println("Testarray values before obj: " + testArray[i]);
                
            }
            
            newArray = s.SortArray(testArray);
            
            for (int i =0; i<testArray.length;i++) {
                
                System.out.println("Testarray values: " + testArray[i]);
                
            }
            
            for (int i =0; i<newArray.length;i++) {
                
                System.out.println("Newarray values: " + newArray[i]);
                
            } */        
        
        
        //Testing rotation of generalpaths: it turns out that rotate can have a minus value
        // ie you don't have to rotate all the way round...
                
              /*   //testing paths again...
                GeneralPath bob = new GeneralPath();
                tx = new AffineTransform();
                
                //make a square
                bob.moveTo(-40,-40);
                bob.lineTo(40,-40);
                bob.lineTo(40,40);
                bob.lineTo(-40,40);
                bob.closePath();
                
                //translate so visible
                tx.translate(400,400);
                bob.transform(tx);
                g2.draw(bob);
                
                //rotate to pi/4;
                tx = new AffineTransform();
                tx.rotate(Math.PI/5,400,400);
                bob.transform(tx);
                g2.draw(bob);
                
                //translate back - in black, so visible
                tx = new AffineTransform();
                tx.rotate(-Math.PI/5,400,400);
                bob.transform(tx);
                g2.setColor(Color.BLACK);
                g2.draw(bob);
                
                PathIterator geoff = bob.getPathIterator(null);
                
                double[] coords = new double[6];
                
                for (int j=0;j<8;j++) {
                    
                    geoff.currentSegment(coords);
                    
                    System.out.println("Segment coords=: "+coords[0] +","+coords[1]);
                    
                    geoff.next();
                    
                    
                }*/
        
    }
}
    
    
    

