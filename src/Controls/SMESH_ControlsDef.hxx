// Copyright (C) 2007-2025  CEA, EDF, OPEN CASCADE
//
// Copyright (C) 2003-2007  OPEN CASCADE, EADS/CCR, LIP6, CEA/DEN,
// CEDRAT, EDF R&D, LEG, PRINCIPIA R&D, BUREAU VERITAS
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
//
// See http://www.salome-platform.org/ or email : webmaster.salome@opencascade.com
//

#ifndef _SMESH_CONTROLSDEF_HXX_
#define _SMESH_CONTROLSDEF_HXX_

#include "SMESH_Controls.hxx"

#include "SMESH_TypeDefs.hxx"
#include "SMESH_ControlsClassifier.hxx"

#include <Bnd_B3d.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <Quantity_Color.hxx>
#include <TColStd_MapOfInteger.hxx>
#include <TCollection_AsciiString.hxx>
#include <NCollection_Map.hxx>
#include <TopAbs.hxx>
#include <TopoDS_Face.hxx>
#include <gp_XYZ.hxx>

#include <set>
#include <map>
#include <vector>

#include <boost/shared_ptr.hpp>

class SMDS_MeshElement;
class SMDS_MeshFace;
class SMDS_MeshNode;
class SMDS_Mesh;

class SMESHDS_Mesh;
class SMESHDS_SubMesh;
class SMESHDS_GroupBase;

class BRepClass3d_SolidClassifier;
class ShapeAnalysis_Surface;
class gp_Pln;
class gp_Pnt;

typedef NCollection_Map< smIdType, smIdHasher > TIDsMap;
typedef NCollection_Sequence<smIdType> TIDsSeq;

namespace SMESH{
  namespace Controls{

    class SMESHCONTROLS_EXPORT TSequenceOfXYZ
    {
      typedef std::vector<gp_XYZ>::size_type size_type;

    public:
      TSequenceOfXYZ();

      explicit TSequenceOfXYZ(size_type n);

      TSequenceOfXYZ(size_type n, const gp_XYZ& t);

      TSequenceOfXYZ(const TSequenceOfXYZ& theSequenceOfXYZ);

      template <class InputIterator>
      TSequenceOfXYZ(InputIterator theBegin, InputIterator theEnd);

      ~TSequenceOfXYZ();

      TSequenceOfXYZ& operator=(const TSequenceOfXYZ& theSequenceOfXYZ);

      gp_XYZ& operator()(size_type n);

      const gp_XYZ& operator()(size_type n) const;

      void clear();

      void reserve(size_type n);

      void push_back(const gp_XYZ& v);

      size_type size() const;


      void setElement(const SMDS_MeshElement* e) { myElem = e; }

      const SMDS_MeshElement* getElement() const { return myElem; }

      SMDSAbs_EntityType getElementEntity() const;

    private:
      std::vector<gp_XYZ>     myArray;
      const SMDS_MeshElement* myElem;
    };

    /*!
     * \brief Class used to detect mesh modification: IsMeshModified() returns
     * true if a mesh has changed since last calling IsMeshModified()
     */
    class SMESHCONTROLS_EXPORT TMeshModifTracer
    {
      unsigned long    myMeshModifTime;
      const SMDS_Mesh* myMesh;
    public:
      TMeshModifTracer();
      void SetMesh( const SMDS_Mesh* theMesh );
      const SMDS_Mesh* GetMesh() const { return myMesh; }
      bool IsMeshModified();
    };

    /*
      Class       : NumericalFunctor
      Description : Root of all Functors returning numeric value
    */
    class SMESHCONTROLS_EXPORT NumericalFunctor: public virtual Functor{
    public:
      NumericalFunctor();
      virtual void SetMesh( const SMDS_Mesh* theMesh );
      virtual double GetValue( long theElementId );
      virtual double GetValue(const TSequenceOfXYZ& /*thePoints*/) { return -1.0;};
      void GetHistogram(int                            nbIntervals,
                        std::vector<int>&              nbEvents,
                        std::vector<double>&           funValues,
                        const std::vector<::smIdType>& elements,
                        const double*                  minmax=0,
                        const bool                     isLogarithmic = false);
      bool IsApplicable( long theElementId ) const;
      virtual bool IsApplicable( const SMDS_MeshElement* element ) const;
      virtual SMDSAbs_ElementType GetType() const = 0;
      virtual double GetBadRate( double Value, int nbNodes ) const = 0;
      long  GetPrecision() const;
      void  SetPrecision( const long thePrecision );
      double Round( const double & value );
      
      bool GetPoints(const ::smIdType theId, TSequenceOfXYZ& theRes) const;
      static bool GetPoints(const SMDS_MeshElement* theElem, TSequenceOfXYZ& theRes);
    protected:
      const SMDS_Mesh*        myMesh;
      const SMDS_MeshElement* myCurrElement;
      long                    myPrecision;
      double                  myPrecisionValue;
    };


    /*
      Class       : Volume
      Description : Functor calculating volume of 3D mesh element
    */
    class SMESHCONTROLS_EXPORT Volume: public virtual NumericalFunctor{
    public:
      virtual double GetValue( long theElementId );
      //virtual double GetValue( const TSequenceOfXYZ& thePoints );
      virtual double GetBadRate( double Value, int nbNodes ) const;
      virtual SMDSAbs_ElementType GetType() const;
    };
  
  
    /*
      Class       : MaxElementLength2D
      Description : Functor calculating maximum length of 2D element
    */
    class SMESHCONTROLS_EXPORT MaxElementLength2D: public virtual NumericalFunctor{
    public:
      virtual double GetValue( long theElementId );
      virtual double GetValue( const TSequenceOfXYZ& P );
      virtual double GetBadRate( double Value, int nbNodes ) const;
      virtual SMDSAbs_ElementType GetType() const;
    };


    /*
      Class       : MaxElementLength3D
      Description : Functor calculating maximum length of 3D element
    */
    class SMESHCONTROLS_EXPORT MaxElementLength3D: public virtual NumericalFunctor{
    public:
      virtual double GetValue( long theElementId );
      virtual double GetBadRate( double Value, int nbNodes ) const;
      virtual SMDSAbs_ElementType GetType() const;
    };
  
  
    /*
      Class       : SMESH_MinimumAngle
      Description : Functor for calculation of minimum angle
    */
    class SMESHCONTROLS_EXPORT MinimumAngle: public virtual NumericalFunctor{
    public:
      virtual double GetValue( const TSequenceOfXYZ& thePoints );
      virtual double GetBadRate( double Value, int nbNodes ) const;
      virtual SMDSAbs_ElementType GetType() const;
    };
  
  
    /*
      Class       : AspectRatio
      Description : Functor for calculating aspect ratio
    */
    class SMESHCONTROLS_EXPORT AspectRatio: public virtual NumericalFunctor{
    public:
      virtual double GetValue( long theElementId );
      virtual double GetValue( const TSequenceOfXYZ& thePoints );
      virtual double GetBadRate( double Value, int nbNodes ) const;
      virtual SMDSAbs_ElementType GetType() const;
      virtual bool IsApplicable( const SMDS_MeshElement* element ) const;
    };
  
  
    /*
      Class       : AspectRatio3D
      Description : Functor for calculating aspect ratio of 3D elems.
    */
    class SMESHCONTROLS_EXPORT AspectRatio3D: public virtual NumericalFunctor{
    public:
      virtual double GetValue( long theElementId );
      virtual double GetValue( const TSequenceOfXYZ& thePoints );
      virtual double GetBadRate( double Value, int nbNodes ) const;
      virtual SMDSAbs_ElementType GetType() const;
      virtual bool IsApplicable( const SMDS_MeshElement* element ) const;
    };
  
  
    /*
      Class       : Warping
      Description : Functor for calculating warping
    */
    class SMESHCONTROLS_EXPORT Warping: public virtual NumericalFunctor{
    public:
      virtual double GetValue( const TSequenceOfXYZ& thePoints );
      virtual double GetBadRate( double Value, int nbNodes ) const;
      virtual SMDSAbs_ElementType GetType() const;
      virtual bool IsApplicable( const SMDS_MeshElement* element ) const;

    protected:
      double ComputeA( const gp_XYZ&, const gp_XYZ&, const gp_XYZ&, const gp_XYZ& ) const;
      double ComputeValue( const TSequenceOfXYZ& thePoints ) const;
    };

    /*
      Class       : Warping3D
      Description : Functor for calculating warping
    */
    class SMESHCONTROLS_EXPORT Warping3D: public virtual Warping {
    public:
      virtual bool IsApplicable(const SMDS_MeshElement* element) const;
      virtual double GetValue(const TSequenceOfXYZ& thePoints);
      virtual double GetValue(long theId);
      virtual SMDSAbs_ElementType GetType() const;
      
      struct Value {
        double myWarp;
        std::vector<long> myPntIds;
        bool operator<(const Value& x) const;
      };

      typedef std::vector<Value> WValues;
      void GetValues(WValues& theValues);

    private:
      void ProcessVolumeELement(WValues& theValues);
    };
    typedef boost::shared_ptr<Warping3D> Warping3DPtr;
  
  
    /*
      Class       : Taper
      Description : Functor for calculating taper
    */
    class SMESHCONTROLS_EXPORT Taper: public virtual NumericalFunctor{
    public:
      virtual double GetValue( const TSequenceOfXYZ& thePoints );
      virtual double GetBadRate( double Value, int nbNodes ) const;
      virtual SMDSAbs_ElementType GetType() const;
      virtual bool IsApplicable( const SMDS_MeshElement* element ) const;
    };

    /*
      Class       : Skew
      Description : Functor for calculating skew in degrees
    */
    class SMESHCONTROLS_EXPORT Skew: public virtual NumericalFunctor{
    public:
      virtual double GetValue( const TSequenceOfXYZ& thePoints );
      virtual double GetBadRate( double Value, int nbNodes ) const;
      virtual SMDSAbs_ElementType GetType() const;
      virtual bool IsApplicable( const SMDS_MeshElement* element ) const;
    };


    /*
      Class       : Area
      Description : Functor for calculating area
    */
    class SMESHCONTROLS_EXPORT Area: public virtual NumericalFunctor{
    public:
      virtual double GetValue( const TSequenceOfXYZ& thePoints );
      virtual double GetBadRate( double Value, int nbNodes ) const;
      virtual SMDSAbs_ElementType GetType() const;
    };
  
  
    /*
      Class       : Length
      Description : Functor for calculating length of edge
    */
    class SMESHCONTROLS_EXPORT Length: public virtual NumericalFunctor{
    public:
      virtual double GetValue( const TSequenceOfXYZ& thePoints );
      virtual double GetBadRate( double Value, int nbNodes ) const;
      virtual SMDSAbs_ElementType GetType() const;
    };

    /*
      Class       : Length2D
      Description : Functor for calculating minimal length of edges of element
    */
    class SMESHCONTROLS_EXPORT Length2D: public virtual NumericalFunctor{
    public:
      Length2D( SMDSAbs_ElementType type = SMDSAbs_Face );
      virtual bool   IsApplicable( const SMDS_MeshElement* element ) const;
      virtual double GetValue( const TSequenceOfXYZ& thePoints );
      virtual double GetBadRate( double Value, int nbNodes ) const;
      virtual SMDSAbs_ElementType GetType() const;
      struct Value{
        double myLength;
        long myPntId[2];
        Value(double theLength, long thePntId1, long thePntId2);
        bool operator<(const Value& x) const;
      };
      typedef std::set<Value> TValues;
      void GetValues(TValues& theValues);

    private:
      SMDSAbs_ElementType myType;
    };
    typedef boost::shared_ptr<Length2D> Length2DPtr;

    /*
      Class       : Length2D
      Description : Functor for calculating minimal length of edges of 3D element
    */
    class SMESHCONTROLS_EXPORT Length3D: public virtual Length2D {
    public:
      Length3D();
    };
    typedef boost::shared_ptr<Length3D> Length3DPtr;

    /*
      Class       : Deflection2D
      Description : Functor for calculating distance between a face and geometry
    */
    class SMESHCONTROLS_EXPORT Deflection2D: public virtual NumericalFunctor{
    public:
      virtual void   SetMesh( const SMDS_Mesh* theMesh );
      virtual double GetValue( const TSequenceOfXYZ& thePoints );
      virtual double GetBadRate( double Value, int nbNodes ) const;
      virtual SMDSAbs_ElementType GetType() const;
    private:
      Handle(ShapeAnalysis_Surface) mySurface;
      int                           myShapeIndex;
      boost::shared_ptr<gp_Pln>     myPlane;
    };

    /*
      Class       : MultiConnection
      Description : Functor for calculating number of faces connected to the edge
    */
    class SMESHCONTROLS_EXPORT MultiConnection: public virtual NumericalFunctor{
    public:
      virtual double GetValue( long theElementId );
      virtual double GetValue( const TSequenceOfXYZ& thePoints );
      virtual double GetBadRate( double Value, int nbNodes ) const;
      virtual SMDSAbs_ElementType GetType() const;
    };
    
    /*
      Class       : MultiConnection2D
      Description : Functor for calculating number of faces connected to the edge
    */
    class SMESHCONTROLS_EXPORT MultiConnection2D: public virtual NumericalFunctor{
    public:
      virtual double GetValue( long theElementId );
      virtual double GetValue( const TSequenceOfXYZ& thePoints );
      virtual double GetBadRate( double Value, int nbNodes ) const;
      virtual SMDSAbs_ElementType GetType() const;
      struct Value{
        long myPntId[2];
        Value(long thePntId1, long thePntId2);
        bool operator<(const Value& x) const;
      };
      typedef std::map<Value,int> MValues;

      void GetValues(MValues& theValues);
    };
    typedef boost::shared_ptr<MultiConnection2D> MultiConnection2DPtr;

    /*
      Class       : BallDiameter
      Description : Functor returning diameter of a ball element
    */
    class SMESHCONTROLS_EXPORT BallDiameter: public virtual NumericalFunctor{
    public:
      virtual double GetValue( long theElementId );
      virtual double GetBadRate( double Value, int nbNodes ) const;
      virtual SMDSAbs_ElementType GetType() const;
    };
    
    /*
      Class       : NodeConnectivityNumber
      Description : Functor returning number of elements connected to a node
    */
    class SMESHCONTROLS_EXPORT NodeConnectivityNumber: public virtual NumericalFunctor{
    public:
      virtual double GetValue( long theNodeId );
      virtual double GetBadRate( double Value, int nbNodes ) const;
      virtual SMDSAbs_ElementType GetType() const;
    };
    
    /*
      Class       : ScaledJacobian
      Description : Functor returning the ScaledJacobian as implemented in VTK for volumetric elements
    */
    class SMESHCONTROLS_EXPORT ScaledJacobian: public virtual NumericalFunctor{
    public:
      virtual double GetValue( long theNodeId );
      virtual double GetBadRate( double Value, int nbNodes ) const;
      virtual SMDSAbs_ElementType GetType() const;
    };


    /*
      PREDICATES
    */
    /*
      Class       : CoincidentNodes
      Description : Predicate of Coincident Nodes
      Note        : This class is suitable only for visualization of Coincident Nodes
    */
    class SMESHCONTROLS_EXPORT CoincidentNodes: public Predicate {
    public:
      CoincidentNodes();
      //virtual Predicate* clone() const { return new CoincidentNodes( *this ); }
      virtual void SetMesh( const SMDS_Mesh* theMesh );
      virtual bool IsSatisfy( long theElementId );
      virtual SMDSAbs_ElementType GetType() const;

      void SetTolerance (const double theToler);
      double GetTolerance () const { return myToler; }

    private:
      double           myToler;
      TIDsMap          myCoincidentIDs;
      TMeshModifTracer myMeshModifTracer;
    };
    typedef boost::shared_ptr<CoincidentNodes> CoincidentNodesPtr;
   
    /*
      Class       : CoincidentElements
      Description : Predicate of Coincident Elements
      Note        : This class is suitable only for visualization of Coincident Elements
    */
    class SMESHCONTROLS_EXPORT CoincidentElements: public Predicate {
    public:
      CoincidentElements();
      virtual void SetMesh( const SMDS_Mesh* theMesh );
      virtual bool IsSatisfy( long theElementId );

    private:
      const SMDS_Mesh* myMesh;
    };
    class SMESHCONTROLS_EXPORT CoincidentElements1D: public CoincidentElements {
    public:
      virtual SMDSAbs_ElementType GetType() const;
      //virtual Predicate* clone() const { return new CoincidentElements1D( *this ); }
    };
    class SMESHCONTROLS_EXPORT CoincidentElements2D: public CoincidentElements {
    public:
      virtual SMDSAbs_ElementType GetType() const;
      //virtual Predicate* clone() const { return new CoincidentElements2D( *this ); }
    };
    class SMESHCONTROLS_EXPORT CoincidentElements3D: public CoincidentElements {
    public:
      virtual SMDSAbs_ElementType GetType() const;
      //virtual Predicate* clone() const { return new CoincidentElements3D( *this ); }
    };

    /*
      Class       : FreeBorders
      Description : Predicate for free borders
    */
    class SMESHCONTROLS_EXPORT FreeBorders: public virtual Predicate{
    public:
      FreeBorders();
      //virtual Predicate* clone() const { return new FreeBorders( *this ); }
      virtual void SetMesh( const SMDS_Mesh* theMesh );
      virtual bool IsSatisfy( long theElementId );
      virtual SMDSAbs_ElementType GetType() const;

    protected:
      const SMDS_Mesh* myMesh;
    };
   

    /*
      Class       : BadOrientedVolume
      Description : Predicate bad oriented volumes
    */
    class SMESHCONTROLS_EXPORT BadOrientedVolume: public virtual Predicate{
    public:
      BadOrientedVolume();
      //virtual Predicate* clone() const { return new BadOrientedVolume( *this ); }
      virtual void SetMesh( const SMDS_Mesh* theMesh );
      virtual bool IsSatisfy( long theElementId );
      virtual SMDSAbs_ElementType GetType() const;

    protected:
      const SMDS_Mesh* myMesh;
    };

    /*
      Class       : ElemEntityType
      Description : Functor for calculating entity type
    */
    class SMESHCONTROLS_EXPORT ElemEntityType: public virtual Predicate{
      public:
      ElemEntityType();
      //virtual Predicate*   clone() const { return new ElemEntityType( *this ); }
      virtual void         SetMesh( const SMDS_Mesh* theMesh );
      virtual bool         IsSatisfy( long theElementId );
      void                 SetType( SMDSAbs_ElementType theType );
      virtual              SMDSAbs_ElementType GetType() const;
      void                 SetElemEntityType( SMDSAbs_EntityType theEntityType );
      SMDSAbs_EntityType   GetElemEntityType() const;

    private:
      const SMDS_Mesh*     myMesh;
      SMDSAbs_ElementType  myType;
      SMDSAbs_EntityType   myEntityType;
    };
    typedef boost::shared_ptr<ElemEntityType> ElemEntityTypePtr;


    /*
      BareBorderVolume
    */
    class SMESHCONTROLS_EXPORT BareBorderVolume: public Predicate
    {
    public:
      BareBorderVolume():myMesh(0) {}
      virtual Predicate* clone() const { return new BareBorderVolume( *this ); }
      virtual void SetMesh( const SMDS_Mesh* theMesh ) { myMesh = theMesh; }
      virtual SMDSAbs_ElementType GetType() const      { return SMDSAbs_Volume; }
      virtual bool IsSatisfy( long theElementId );
    protected:
      const SMDS_Mesh* myMesh;
    };
    typedef boost::shared_ptr<BareBorderVolume> BareBorderVolumePtr;

    /*
      BareBorderFace
    */
    class SMESHCONTROLS_EXPORT BareBorderFace: public Predicate
    {
    public:
      BareBorderFace():myMesh(0) {}
      //virtual Predicate* clone() const { return new BareBorderFace( *this ); }
      virtual void SetMesh( const SMDS_Mesh* theMesh ) { myMesh = theMesh; }
      virtual SMDSAbs_ElementType GetType() const      { return SMDSAbs_Face; }
      virtual bool IsSatisfy( long theElementId );
    protected:
      const SMDS_Mesh* myMesh;
      std::vector< const SMDS_MeshNode* > myLinkNodes;
    };
    typedef boost::shared_ptr<BareBorderFace> BareBorderFacePtr;

    /*
      OverConstrainedVolume
    */
    class SMESHCONTROLS_EXPORT OverConstrainedVolume: public Predicate
    {
    public:
      OverConstrainedVolume():myMesh(0) {}
      virtual Predicate* clone() const { return new OverConstrainedVolume( *this ); }
      virtual void SetMesh( const SMDS_Mesh* theMesh ) { myMesh = theMesh; }
      virtual SMDSAbs_ElementType GetType() const      { return SMDSAbs_Volume; }
      virtual bool IsSatisfy( long theElementId );
    protected:
      const SMDS_Mesh* myMesh;
    };
    typedef boost::shared_ptr<OverConstrainedVolume> OverConstrainedVolumePtr;

    /*
      OverConstrainedFace
    */
    class SMESHCONTROLS_EXPORT OverConstrainedFace: public Predicate
    {
    public:
      OverConstrainedFace():myMesh(0) {}
      //virtual Predicate* clone() const { return new OverConstrainedFace( *this ); }
      virtual void SetMesh( const SMDS_Mesh* theMesh ) { myMesh = theMesh; }
      virtual SMDSAbs_ElementType GetType() const      { return SMDSAbs_Face; }
      virtual bool IsSatisfy( long theElementId );
    protected:
      const SMDS_Mesh* myMesh;
    };
    typedef boost::shared_ptr<OverConstrainedFace> OverConstrainedFacePtr;

    /*
      Class       : FreeEdges
      Description : Predicate for free Edges
    */
    class SMESHCONTROLS_EXPORT FreeEdges: public virtual Predicate{
    public:
      FreeEdges();
      //virtual Predicate* clone() const { return new FreeEdges( *this ); }
      virtual void SetMesh( const SMDS_Mesh* theMesh );
      virtual bool IsSatisfy( long theElementId );
      virtual SMDSAbs_ElementType GetType() const;
      static bool IsFreeEdge( const SMDS_MeshNode** theNodes, const ::smIdType theFaceId  );
      typedef long TElemId;
      struct Border{
        TElemId myElemId;
        TElemId myPntId[2];
        Border(long theElemId, long thePntId1, long thePntId2);
        bool operator<(const Border& x) const;
      };
      typedef std::set<Border> TBorders;
      void GetBoreders(TBorders& theBorders);

    protected:
      const SMDS_Mesh* myMesh;
    };
    typedef boost::shared_ptr<FreeEdges> FreeEdgesPtr;
    
    
    /*
      Class       : FreeNodes
      Description : Predicate for free nodes
    */
    class SMESHCONTROLS_EXPORT FreeNodes: public virtual Predicate{
    public:
      FreeNodes();
      //virtual Predicate* clone() const { return new FreeNodes( *this ); }
      virtual void SetMesh( const SMDS_Mesh* theMesh );
      virtual bool IsSatisfy( long theNodeId );
      virtual SMDSAbs_ElementType GetType() const;

    protected:
      const SMDS_Mesh* myMesh;
    };
    

    /*
      Class       : RangeOfIds
      Description : Predicate for Range of Ids.
                    Range may be specified with two ways.
                    1. Using AddToRange method
                    2. With SetRangeStr method. Parameter of this method is a string
                       like as "1,2,3,50-60,63,67,70-"
    */
    class SMESHCONTROLS_EXPORT RangeOfIds: public virtual Predicate
    {
    public:
      RangeOfIds();
      //virtual Predicate*            clone() const { return new RangeOfIds( *this ); }
      virtual void                  SetMesh( const SMDS_Mesh* theMesh );
      virtual bool                  IsSatisfy( long theNodeId );
      virtual SMDSAbs_ElementType   GetType() const;
      virtual void                  SetType( SMDSAbs_ElementType theType );

      bool                          AddToRange( long theEntityId );
      void                          GetRangeStr( TCollection_AsciiString& );
      bool                          SetRangeStr( const TCollection_AsciiString& );

    protected:
      const SMDS_Mesh*              myMesh;

      std::vector< ::smIdType >       myMin;
      std::vector< ::smIdType >       myMax;
      TIDsMap                       myIds;

      SMDSAbs_ElementType           myType;
    };
    
    typedef boost::shared_ptr<RangeOfIds> RangeOfIdsPtr;
   
    
    /*
      Class       : Comparator
      Description : Base class for comparators
    */
    class SMESHCONTROLS_EXPORT Comparator: public virtual Predicate{
    public:
      Comparator();
      virtual ~Comparator();
      virtual void SetMesh( const SMDS_Mesh* theMesh );
      virtual void SetMargin(double theValue);
      virtual void SetNumFunctor(NumericalFunctorPtr theFunct);
      virtual bool IsSatisfy( long theElementId ) = 0;
      virtual SMDSAbs_ElementType GetType() const;
      double  GetMargin();
  
    protected:
      double myMargin;
      NumericalFunctorPtr myFunctor;
    };
    typedef boost::shared_ptr<Comparator> ComparatorPtr;
  
  
    /*
      Class       : LessThan
      Description : Comparator "<"
    */
    class SMESHCONTROLS_EXPORT LessThan: public virtual Comparator{
    public:
      virtual bool IsSatisfy( long theElementId );
      //virtual Predicate* clone() const { return new LessThan( *this ); }
    };
  
  
    /*
      Class       : MoreThan
      Description : Comparator ">"
    */
    class SMESHCONTROLS_EXPORT MoreThan: public virtual Comparator{
    public:
      virtual bool IsSatisfy( long theElementId );
      //virtual Predicate* clone() const { return new MoreThan( *this ); }
    };
  
  
    /*
      Class       : EqualTo
      Description : Comparator "="
    */
    class SMESHCONTROLS_EXPORT EqualTo: public virtual Comparator{
    public:
      EqualTo();
      //virtual Predicate* clone() const { return new EqualTo( *this ); }
      virtual bool IsSatisfy( long theElementId );
      virtual void SetTolerance( double theTol );
      virtual double GetTolerance();
  
    private:
      double myToler;
    };
    typedef boost::shared_ptr<EqualTo> EqualToPtr;
  
    
    /*
      Class       : LogicalNOT
      Description : Logical NOT predicate
    */
    class SMESHCONTROLS_EXPORT LogicalNOT: public virtual Predicate{
    public:
      LogicalNOT();
      //virtual Predicate* clone() const { return new LogicalNOT( *this ); }
      virtual ~LogicalNOT();
      virtual bool IsSatisfy( long theElementId );
      virtual void SetMesh( const SMDS_Mesh* theMesh );
      virtual void SetPredicate(PredicatePtr thePred);
      virtual SMDSAbs_ElementType GetType() const;
  
    private:
      PredicatePtr myPredicate;
    };
    typedef boost::shared_ptr<LogicalNOT> LogicalNOTPtr;
    
  
    /*
      Class       : LogicalBinary
      Description : Base class for binary logical predicate
    */
    class SMESHCONTROLS_EXPORT LogicalBinary: public virtual Predicate{
    public:
      LogicalBinary();
      virtual ~LogicalBinary();
      virtual void SetMesh( const SMDS_Mesh* theMesh );
      virtual void SetPredicate1(PredicatePtr thePred);
      virtual void SetPredicate2(PredicatePtr thePred);
      virtual SMDSAbs_ElementType GetType() const;
  
    protected:
      PredicatePtr myPredicate1;
      PredicatePtr myPredicate2;
    };
    typedef boost::shared_ptr<LogicalBinary> LogicalBinaryPtr;
  
  
    /*
      Class       : LogicalAND
      Description : Logical AND
    */
    class SMESHCONTROLS_EXPORT LogicalAND: public virtual LogicalBinary{
    public:
      virtual bool IsSatisfy( long theElementId );
      //virtual Predicate* clone() const { return new LogicalAND( *this ); }
    };
  
  
    /*
      Class       : LogicalOR
      Description : Logical OR
    */
    class SMESHCONTROLS_EXPORT LogicalOR: public virtual LogicalBinary{
    public:
      virtual bool IsSatisfy( long theElementId );
      //virtual Predicate* clone() const { return new LogicalOR( *this ); }
    };
  
  
    /*
      Class       : ManifoldPart
      Description : Predicate for manifold part of mesh
    */
    class SMESHCONTROLS_EXPORT ManifoldPart: public virtual Predicate{
    public:

      /* internal class for algorithm uses */
      class Link
      {
      public:
        Link( SMDS_MeshNode* theNode1,
              SMDS_MeshNode* theNode2 );
        ~Link();
        
        bool IsEqual( const ManifoldPart::Link& theLink ) const;
        bool operator<(const ManifoldPart::Link& x) const;
        
        SMDS_MeshNode* myNode1;
        SMDS_MeshNode* myNode2;
      };

      bool IsEqual( const ManifoldPart::Link& theLink1,
                    const ManifoldPart::Link& theLink2 );
      
      typedef std::set<ManifoldPart::Link>                TMapOfLink;
      typedef std::vector<SMDS_MeshFace*>                 TVectorOfFacePtr;
      typedef std::vector<ManifoldPart::Link>             TVectorOfLink;
      typedef std::map<SMDS_MeshFace*,int>                TDataMapFacePtrInt;
      typedef std::map<ManifoldPart::Link,SMDS_MeshFace*> TDataMapOfLinkFacePtr;
      
      ManifoldPart();
      ~ManifoldPart();
      //virtual Predicate* clone() const { return new ManifoldPart( *this ); }
      virtual void SetMesh( const SMDS_Mesh* theMesh );
      // invoke when all parameters already set
      virtual bool IsSatisfy( long theElementId );
      virtual      SMDSAbs_ElementType GetType() const;

      void    SetAngleTolerance( const double theAngToler );
      double  GetAngleTolerance() const;
      void    SetIsOnlyManifold( const bool theIsOnly );
      void    SetStartElem( const long  theStartElemId );

    private:
      bool    process();
      bool    findConnected( const TDataMapFacePtrInt& theAllFacePtrInt,
                             SMDS_MeshFace*            theStartFace,
                             TMapOfLink&               theNonManifold,
                             TIDsMap&                  theResFaces );
      bool    isInPlane( const SMDS_MeshFace* theFace1,
                          const SMDS_MeshFace* theFace2 );
      void    expandBoundary( TMapOfLink&            theMapOfBoundary,
                              TVectorOfLink&         theSeqOfBoundary,
                              TDataMapOfLinkFacePtr& theDMapLinkFacePtr,
                              TMapOfLink&            theNonManifold,
                              SMDS_MeshFace*         theNextFace ) const;

      void     getFacesByLink( const Link& theLink,
                               TVectorOfFacePtr& theFaces ) const;

    private:
      const SMDS_Mesh*      myMesh;
      TIDsMap               myMapIds;
      TIDsMap               myMapBadGeomIds;
      TVectorOfFacePtr      myAllFacePtr;
      TDataMapFacePtrInt    myAllFacePtrIntDMap;
      double                myAngToler;
      bool                  myIsOnlyManifold;
      long                  myStartElemId;

    };
    typedef boost::shared_ptr<ManifoldPart> ManifoldPartPtr;

    /*
      Class       : BelongToMeshGroup
      Description : Verify whether a mesh element is included into a mesh group
    */
    class SMESHCONTROLS_EXPORT BelongToMeshGroup : public virtual Predicate
    {
    public:
      BelongToMeshGroup();
      //virtual Predicate* clone() const { return new BelongToMeshGroup( *this ); }
      virtual void SetMesh( const SMDS_Mesh* theMesh );
      virtual bool IsSatisfy( long theElementId );
      virtual SMDSAbs_ElementType GetType() const;

      void SetGroup( SMESHDS_GroupBase* g );
      void SetStoreName( const std::string& sn );
      const SMESHDS_GroupBase* GetGroup() const { return myGroup; }

    private:
      SMESHDS_GroupBase* myGroup;
      std::string        myStoreName;
    };
    typedef boost::shared_ptr<BelongToMeshGroup> BelongToMeshGroupPtr;

    /*
      Class       : ElementsOnSurface
      Description : Predicate elements that lying on indicated surface
                    (plane or cylinder)
    */
    class SMESHCONTROLS_EXPORT ElementsOnSurface : public virtual Predicate {
    public:
      ElementsOnSurface();
      ~ElementsOnSurface();
      //virtual Predicate* clone() const { return new ElementsOnSurface( *this ); }
      virtual void SetMesh( const SMDS_Mesh* theMesh );
      virtual bool IsSatisfy( long theElementId );
      virtual      SMDSAbs_ElementType GetType() const;

      void    SetTolerance( const double theToler );
      double  GetTolerance() const;
      void    SetSurface( const TopoDS_Shape& theShape,
                          const SMDSAbs_ElementType theType );
      void    SetUseBoundaries( bool theUse );
      bool    GetUseBoundaries() const { return myUseBoundaries; }

    private:
      void    process();
      void    process( const SMDS_MeshElement* theElem  );
      bool    isOnSurface( const SMDS_MeshNode* theNode );

    private:
      TMeshModifTracer      myMeshModifTracer;
      TIDsMap               myIds;
      SMDSAbs_ElementType   myType;
      TopoDS_Face           mySurf;
      double                myToler;
      bool                  myUseBoundaries;
      GeomAPI_ProjectPointOnSurf myProjector;
    };

    typedef boost::shared_ptr<ElementsOnSurface> ElementsOnSurfacePtr;


    /*
      Class       : ElementsOnShape
      Description : Predicate elements that lying on indicated shape
                    (1D, 2D or 3D)
    */
    class SMESHCONTROLS_EXPORT ElementsOnShape : public Predicate
    {
    public:
      ElementsOnShape();
      ~ElementsOnShape();

      virtual Predicate* clone() const;
      virtual void SetMesh (const SMDS_Mesh* theMesh);
      virtual bool IsSatisfy (long theElementId);
      virtual SMDSAbs_ElementType GetType() const;

      void    SetTolerance (const double theToler);
      double  GetTolerance() const;
      void    SetAllNodes (bool theAllNodes);
      bool    GetAllNodes() const { return myAllNodesFlag; }
      void    SetShape (const TopoDS_Shape& theShape,
                        const SMDSAbs_ElementType theType);
      bool    IsSatisfy (const SMDS_MeshElement* elem);
      bool    IsSatisfy (const SMDS_MeshNode* node, TopoDS_Shape* okShape=0);
      void    GetParams( double & u, double & v ) const { u = myU; v = myV; }

    private:

      struct OctreeClassifier;

      void clearClassifiers();
      bool getNodeIsOut( const SMDS_MeshNode* n, bool& isOut );
      void setNodeIsOut( const SMDS_MeshNode* n, bool  isOut );

      std::vector< Classifier >  myClassifiers;
      std::vector< Classifier* > myWorkClassifiers;
      OctreeClassifier*          myOctree;
      SMDSAbs_ElementType        myType;
      TopoDS_Shape               myShape;
      double                     myToler;
      double                     myU, myV; // result of node projection on EDGE or FACE
      bool                       myAllNodesFlag;

      TMeshModifTracer           myMeshModifTracer;
      std::vector<bool>          myNodeIsChecked;
      std::vector<bool>          myNodeIsOut;
    };

    typedef boost::shared_ptr<ElementsOnShape> ElementsOnShapePtr;


    /*
      Class       : BelongToGeom
      Description : Predicate for verifying whether entity belong to
      specified geometrical support
    */
    class SMESHCONTROLS_EXPORT BelongToGeom: public virtual Predicate
    {
    public:
      BelongToGeom();
      virtual Predicate* clone() const;

      virtual void                    SetMesh( const SMDS_Mesh* theMesh );
      virtual void                    SetGeom( const TopoDS_Shape& theShape );

      virtual bool                    IsSatisfy( long theElementId );

      virtual void                    SetType( SMDSAbs_ElementType theType );
      virtual                         SMDSAbs_ElementType GetType() const;

      TopoDS_Shape                    GetShape();
      const SMESHDS_Mesh*             GetMeshDS() const;

      void                            SetTolerance( double );
      double                          GetTolerance();

    private:
      virtual void                    init();

      TopoDS_Shape                    myShape;
      TColStd_MapOfInteger            mySubShapesIDs;
      const SMESHDS_Mesh*             myMeshDS;
      SMDSAbs_ElementType             myType;
      bool                            myIsSubshape;
      double                          myTolerance;          // only if myIsSubshape == false
      Controls::ElementsOnShapePtr    myElementsOnShapePtr; // only if myIsSubshape == false
    };
    typedef boost::shared_ptr<BelongToGeom> BelongToGeomPtr;

    /*
      Class       : LyingOnGeom
      Description : Predicate for verifying whether entity lying or partially lying on
      specified geometrical support
    */
    class SMESHCONTROLS_EXPORT LyingOnGeom: public virtual Predicate
    {
    public:
      LyingOnGeom();
      virtual Predicate* clone() const;
      
      virtual void                    SetMesh( const SMDS_Mesh* theMesh );
      virtual void                    SetGeom( const TopoDS_Shape& theShape );
      
      virtual bool                    IsSatisfy( long theElementId );
      
      virtual void                    SetType( SMDSAbs_ElementType theType );
      virtual                         SMDSAbs_ElementType GetType() const;
      
      TopoDS_Shape                    GetShape();
      const SMESHDS_Mesh*             GetMeshDS() const;

      void                            SetTolerance( double );
      double                          GetTolerance();
      
   private:
      virtual void                    init();

      TopoDS_Shape                    myShape;
      TColStd_MapOfInteger            mySubShapesIDs;
      const SMESHDS_Mesh*             myMeshDS;
      SMDSAbs_ElementType             myType;
      bool                            myIsSubshape;
      double                          myTolerance;          // only if myIsSubshape == false
      Controls::ElementsOnShapePtr    myElementsOnShapePtr; // only if myIsSubshape == false
    };
    typedef boost::shared_ptr<LyingOnGeom> LyingOnGeomPtr;

    /*
      Class       : FreeFaces
      Description : Predicate for free faces
    */
    class SMESHCONTROLS_EXPORT FreeFaces: public virtual Predicate{
    public:
      FreeFaces();
      //virtual Predicate* clone() const { return new FreeFaces( *this ); }
      virtual void SetMesh( const SMDS_Mesh* theMesh );
      virtual bool IsSatisfy( long theElementId );
      virtual SMDSAbs_ElementType GetType() const;

    private:
      const SMDS_Mesh* myMesh;
    };

    /*
      Class       : LinearOrQuadratic
      Description : Predicate for free faces
    */
    class SMESHCONTROLS_EXPORT LinearOrQuadratic: public virtual Predicate{
    public:
      LinearOrQuadratic();
      //virtual Predicate*  clone() const { return new LinearOrQuadratic( *this ); }
      virtual void        SetMesh( const SMDS_Mesh* theMesh );
      virtual bool        IsSatisfy( long theElementId );
      void                SetType( SMDSAbs_ElementType theType );
      virtual SMDSAbs_ElementType GetType() const;

    private:
      const SMDS_Mesh*    myMesh;
      SMDSAbs_ElementType myType;
    };
    typedef boost::shared_ptr<LinearOrQuadratic> LinearOrQuadraticPtr;

    /*
      Class       : GroupColor
      Description : Functor for check color of group to which mesh element belongs to
    */
    class SMESHCONTROLS_EXPORT GroupColor: public virtual Predicate{
    public:
      GroupColor();
      //virtual Predicate*  clone() const { return new GroupColor( *this ); }
      virtual void        SetMesh( const SMDS_Mesh* theMesh );
      virtual bool        IsSatisfy( long theElementId );
      void                SetType( SMDSAbs_ElementType theType );
      virtual             SMDSAbs_ElementType GetType() const;
      void                SetColorStr( const TCollection_AsciiString& );
      void                GetColorStr( TCollection_AsciiString& ) const;
      
    private:
      typedef std::set< long > TIDs;

      Quantity_Color      myColor;
      SMDSAbs_ElementType myType;
      TIDs                myIDs;
    };
    typedef boost::shared_ptr<GroupColor> GroupColorPtr;

    /*
      Class       : ElemGeomType
      Description : Predicate to check element geometry type
    */
    class SMESHCONTROLS_EXPORT ElemGeomType: public virtual Predicate{
    public:
      ElemGeomType();
      //virtual Predicate*   clone() const { return new ElemGeomType( *this ); }
      virtual void         SetMesh( const SMDS_Mesh* theMesh );
      virtual bool         IsSatisfy( long theElementId );
      void                 SetType( SMDSAbs_ElementType theType );
      virtual              SMDSAbs_ElementType GetType() const;
      void                 SetGeomType( SMDSAbs_GeometryType theType );
      SMDSAbs_GeometryType GetGeomType() const;

    private:
      const SMDS_Mesh*     myMesh;
      SMDSAbs_ElementType  myType;
      SMDSAbs_GeometryType myGeomType;
    };
    typedef boost::shared_ptr<ElemGeomType> ElemGeomTypePtr;

    /*
      Class       : CoplanarFaces
      Description : Predicate to check angle between faces
    */
    class SMESHCONTROLS_EXPORT CoplanarFaces: public virtual Predicate
    {
    public:
      CoplanarFaces();
      //virtual Predicate*   clone() const { return new CoplanarFaces( *this ); }
      void                 SetFace( long theID )                   { myFaceID = theID; }
      long                 GetFace() const                         { return myFaceID; }
      void                 SetTolerance (const double theToler)    { myToler = theToler; }
      double               GetTolerance () const                   { return myToler; }
      virtual              SMDSAbs_ElementType GetType() const     { return SMDSAbs_Face; }

      virtual void         SetMesh( const SMDS_Mesh* theMesh );
      virtual bool         IsSatisfy( long theElementId );

    private:
      TMeshModifTracer     myMeshModifTracer;
      long                 myFaceID;
      double               myToler;
      TIDsMap              myCoplanarIDs;
    };
    typedef boost::shared_ptr<CoplanarFaces> CoplanarFacesPtr;

    /*
      Class       : ConnectedElements
      Description : Predicate to get elements of one domain
    */
    class SMESHCONTROLS_EXPORT ConnectedElements: public virtual Predicate
    {
    public:
      ConnectedElements();
      //virtual Predicate*   clone() const { return new ConnectedElements( *this ); }
      void                 SetNode( ::smIdType nodeID );
      void                 SetPoint( double x, double y, double z );
      ::smIdType           GetNode() const;
      std::vector<double>  GetPoint() const;

      void                 SetType( SMDSAbs_ElementType theType );
      virtual              SMDSAbs_ElementType GetType() const;

      virtual void         SetMesh( const SMDS_Mesh* theMesh );
      virtual bool         IsSatisfy( long theElementId );

      //const std::set<long>& GetDomainIDs() const { return myOkIDs; }

    private:
      ::smIdType            myNodeID;
      std::vector<double>   myXYZ;
      SMDSAbs_ElementType   myType;
      TMeshModifTracer      myMeshModifTracer;

      void                  clearOkIDs();
      bool                  myOkIDsReady;
      std::set<::smIdType>  myOkIDs; // empty means that there is one domain
    };
    typedef boost::shared_ptr<ConnectedElements> ConnectedElementsPtr;

    /*
      FILTER
    */
    class SMESHCONTROLS_EXPORT Filter {
    public:
      Filter();
      virtual ~Filter();
      virtual void SetPredicate(PredicatePtr thePred);

      typedef std::vector<long> TIdSequence;

      virtual
      void
      GetElementsId( const SMDS_Mesh*     theMesh,
                     TIdSequence&         theSequence,
                     SMDS_ElemIteratorPtr theElements=0);

      static
      void
      GetElementsId( const SMDS_Mesh*     theMesh,
                     PredicatePtr         thePredicate,
                     TIdSequence&         theSequence,
                     SMDS_ElemIteratorPtr theElements=0 );
      
    protected:
      PredicatePtr myPredicate;
    };
  }
}


#endif
