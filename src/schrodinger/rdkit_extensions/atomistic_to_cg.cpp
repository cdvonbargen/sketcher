#include "schrodinger/rdkit_extensions/cg_conversions.h"

#include <queue>
#include <unordered_map>
#include <memory>
#include <fmt/format.h>

#include <fmt/format.h>
#include <rdkit/GraphMol/RWMol.h>
#include <rdkit/GraphMol/ROMol.h>
#include <rdkit/GraphMol/MolOps.h>
#include <rdkit/GraphMol/MonomerInfo.h>
#include <rdkit/GraphMol/SmilesParse/SmilesParse.h>
#include <rdkit/GraphMol/SmilesParse/SmilesWrite.h>
#include <rdkit/GraphMol/Substruct/SubstructMatch.h>

#include "schrodinger/rdkit_extensions/cg_monomer_database.h"
#include "schrodinger/rdkit_extensions/coarse_grain.h"
#include "schrodinger/rdkit_extensions/helm.h"
#include "schrodinger/rdkit_extensions/molops.h"

namespace schrodinger
{
namespace rdkit_extensions
{

namespace
{

const std::string ATTACH_FROM{"attachFrom"};
const std::string MONOMER_IDX1{"monomerIndex1"};
const std::string MONOMER_IDX2{"monomerIndex2"};
const std::string REFERENCE_IDX{"referenceIndex"};

constexpr int SIDECHAIN_IDX = 2;
constexpr int MIN_ATTCHPTS = 2;
constexpr auto NO_ATTACHMENT = std::numeric_limits<unsigned int>::max();

// std::map to allow sequential/ordered iteration
using ChainsAndResidues =
    std::map<std::string,
             std::map<std::pair<int, std::string>, std::vector<unsigned int>>>;

// attachment points 1 and 2 are backbone attachment points
// and 3 is the side chain attachment point
const std::string GENERIC_AMINO_ACID_QUERY =
    "[NX3,NX4+:1][CX4H]([*:3])[CX3](=[OX1])[O,N:2]";
const std::string GLYCINE_AMINO_ACID_QUERY =
    "[N:1][CX4H2][CX3](=[OX1])[O,N:2]"; // no side chain

// SMILES monomer to CG monomer abbreviation mapping
// temporary for now, for proof of concept
// got most of these from pubchem. include the version with N and O
const std::unordered_map<std::string, std::string> amino_acids = {
    {"CC(N)C(=O)O", "A"},                 // Alanine (Ala)
    {"NC(N)=NCCCC(N)C(=O)O", "R"},        // Arginine (Arg)
    {"NC(=O)CC(N)C(=O)O", "N"},           // Asparagine (Asn)
    {"NC(CC(=O)O)C(=O)O", "D"},           // Aspartic acid (Asp)
    {"NC(CS)C(=O)O", "C"},                // Cysteine (Cys)
    {"NC(=O)CCC(N)C(=O)O", "Q"},          // Glutamine (Gln)
    {"NC(CCC(=O)O)C(=O)O", "E"},          // Glutamic acid (Glu)
    {"NCC(=O)O", "G"},                    // Glycine (Gly)
    {"NC(Cc1cnc[nH]1)C(=O)O", "H"},       // Histidine (His)
    {"CCC(C)C(N)C(=O)O", "I"},            // Isoleucine (Ile)
    {"CC(C)CC(N)C(=O)O", "L"},            // Leucine (Leu)
    {"NCCCCC(N)C(=O)O", "K"},             // Lysine (Lys)
    {"CSCCC(N)C(=O)O", "M"},              // Methionine (Met)
    {"NC(Cc1ccccc1)C(=O)O", "F"},         // Phenylalanine (Phe)
    {"O=C(O)C1CCCN1", "P"},               // Proline (Pro)
    {"NC(CO)C(=O)O", "S"},                // Serine (Ser)
    {"CC(O)C(N)C(=O)O", "T"},             // Threonine (Thr)
    {"NC(Cc1c[nH]c2ccccc12)C(=O)O", "W"}, // Tryptophan (Trp)
    {"NC(Cc1ccc(O)cc1)C(=O)O", "Y"},      // Tyrosine (Tyr)
    {"CC(C)C(N)C(=O)O", "V"},             // Valine (Val)
    {"CC(N)C(N)=O", "A"},
    {"NC(=O)C(N)CCCN=C(N)N", "R"}, // arginine, pubchem version
    {"N=C(N)NCCCC(N)C(N)=O", "R"}, // arginine, from HELM paper examples
                                   // (different double bond placement)
    {"NC(=O)CC(N)C(N)=O", "N"},
    {"NC(=O)C(N)CC(=O)O", "D"},
    {"NC(=O)C(N)CS", "C"},
    {"NC(=O)CCC(N)C(N)=O", "Q"},
    {"NC(=O)C(N)CCC(=O)O", "E"},
    {"NCC(N)=O", "G"},
    {"NC(=O)C(N)Cc1cnc[nH]1", "H"},
    {"CCC(C)C(N)C(N)=O", "I"},
    {"CC(C)CC(N)C(N)=O", "L"},
    {"NCCCCC(N)C(N)=O", "K"},
    {"CSCCC(N)C(N)=O", "M"},
    {"NC(=O)C(N)Cc1ccccc1", "F"},
    {"NC(=O)C1CCCN1", "P"},
    {"NC(=O)C(N)CO", "S"},
    {"CC(O)C(N)C(N)=O", "T"},
    {"NC(=O)C(N)Cc1c[nH]c2ccccc12", "W"},
    {"NC(=O)C(N)Cc1ccc(O)cc1", "Y"},
    {"CC(C)C(N)C(N)=O", "V"}};

// 3-letter to 1-letter amino acid code mapping
// From mmpdb_get_three_to_one_letter_residue_map
// These are not currently in the monomer database, but some have
// symbols that are already in the monomer database. We may need to
// figure out how to have multiple 3-letter codes for a single symbol
// polymer_type pair (Histidine is the best example)
const std::unordered_map<std::string, char> backup_res_table({
    {"ARN", 'R'}, // Neutral-Arginine
    {"ASH", 'D'}, // Protonated Aspartic
    {"GLH", 'E'}, // Protonated Glutamic
    {"HID", 'H'}, // Histidine (protonated at delta N)
    {"HIE", 'H'}, // Histidine (protonated at epsilon N)
    {"HIP", 'H'}, // Histidine (protonated at both N)
    {"HSD", 'H'}, // Histidine (protonated at delta N, CHARMM name)
    {"HSE", 'H'}, // Histidine (protonated at epsilon N, CHARMM name)
    {"HSP", 'H'}, // Histidine (protonated at both N, CHARMM name)
    {"LYN", 'K'}, // Protonated Lysine
    {"SRO", 'S'}, // Ionized Serine
    {"THO", 'T'}, // Ionized Threonine
    {"TYO", 'Y'}, // Ionized Tyrosine
    {"XXX", 'X'}, // Unknown
});

struct Linkage {
    unsigned int monomer_idx1;
    unsigned int monomer_idx2;
    unsigned int attach_from;
    unsigned int attach_to;
    std::string to_string() const
    {
        return fmt::format("R{}-R{}", attach_from, attach_to);
    }
};

bool already_matched(const RDKit::ROMol& mol,
                     const std::vector<unsigned int>& ids)
{
    // Make sure this match hasn't already been accounted for by a previous
    // match
    for (auto id : ids) {
        auto at = mol.getAtomWithIdx(id);
        unsigned int attch;
        if (at->hasProp(MONOMER_IDX1) &&
            at->getPropIfPresent(ATTACH_FROM, attch) &&
            attch == NO_ATTACHMENT) {
            return false;
        }
    }
    return true;
}

/*
 * Function that takes the SMARTS query and atomistic molecule and adds the atom
 * indices of the matches to the monomers vector
 *
 */
void add_matches_to_monomers(
    const std::string& smarts_query, RDKit::ROMol& atomistic_mol,
    std::vector<std::vector<int>>& monomers,
    std::unordered_map<unsigned int, unsigned int>& attch_pts,
    std::vector<Linkage>& linkages)
{
    std::unique_ptr<RDKit::RWMol> query(RDKit::SmartsToMol(smarts_query));
    // maps SMARTS query index to attachment point #
    std::vector<unsigned int> attch_map(query->getNumAtoms(), NO_ATTACHMENT);
    for (const auto atom : query->atoms()) {
        if (atom->hasProp(RDKit::common_properties::molAtomMapNumber)) {
            attch_map[atom->getIdx()] = atom->getProp<unsigned int>(
                RDKit::common_properties::molAtomMapNumber);
        }
    }

    // Set a final function check that ensures the entire match has not
    // already been accounted for by a previous SMARTS search.
    RDKit::SubstructMatchParameters params;
    params.useChirality = false;
    params.extraFinalCheck = &already_matched;
    auto matches = RDKit::SubstructMatch(atomistic_mol, *query, params);

    for (const auto& match : matches) {
        std::vector<int> monomer;
        auto monomer_idx = monomers.size();
        for (const auto& [query_idx, atom_idx] : match) {
            auto atom = atomistic_mol.getAtomWithIdx(atom_idx);
            if (atom->hasProp(MONOMER_IDX1) && atom->hasProp(MONOMER_IDX2)) {
                // This shouldn't happen, sanity check
                throw std::runtime_error(fmt::format(
                    "Atom {} belongs to more than 2 monomers", atom->getIdx()));
            }

            if (atom->hasProp(MONOMER_IDX1)) {
                atom->setProp<unsigned int>(MONOMER_IDX2, monomer_idx);
                // ATTACH_FROM property should be set when MONOMER_IDX1 is set
                auto attach_from_idx = atom->getProp<unsigned int>(ATTACH_FROM);

                // Make sure linkages are directionally correct, so R2-R1 or
                // R3-R1 instead of R1-R2 or R1-R3
                if (attach_from_idx >= attch_map[query_idx]) {
                    Linkage link = {atom->getProp<unsigned int>(MONOMER_IDX1),
                                    static_cast<unsigned int>(monomer_idx),
                                    attach_from_idx, attch_map[query_idx]};
                    linkages.push_back(link);
                } else {
                    Linkage link = {static_cast<unsigned int>(monomer_idx),
                                    atom->getProp<unsigned int>(MONOMER_IDX1),
                                    attch_map[query_idx], attach_from_idx};
                    linkages.push_back(link);
                }
            } else {
                atom->setProp<unsigned int>(MONOMER_IDX1, monomer_idx);
                atom->setProp(ATTACH_FROM, attch_map[query_idx]);
            }
            monomer.push_back(atom_idx);

            // if there is a side chain, the attachment point will be at the
            // SIDECHAIN_IDX and will be indicated by the presence of the atom
            // map number. For now, assume there is a single side chain per
            // monomer
            if (query_idx == SIDECHAIN_IDX &&
                query->getAtomWithIdx(query_idx)->hasProp(
                    RDKit::common_properties::molAtomMapNumber)) {
                attch_pts[monomers.size()] = atom_idx;
            }
        }
        monomers.push_back(monomer);
    }
}

void add_sidechain_to_monomer(const RDKit::ROMol& atomistic_mol,
                              std::vector<int>& monomer,
                              unsigned int monomer_idx,
                              unsigned int attch_at_idx)
{

    // BFS but use MONOMER_IDX as visited marker
    std::queue<unsigned int> q;
    q.push(attch_at_idx);
    while (!q.empty()) {
        auto at_idx = q.front();
        q.pop();
        auto at = atomistic_mol.getAtomWithIdx(at_idx);
        if (!at->hasProp(MONOMER_IDX1)) {
            at->setProp<unsigned int>(MONOMER_IDX1, monomer_idx);
            monomer.push_back(at_idx);
        }
        for (const auto& nbr : atomistic_mol.atomNeighbors(at)) {
            if (!nbr->hasProp(MONOMER_IDX1)) {
                q.push(nbr->getIdx());
            }
        }
    }
}

/*
 * Break an atomistic molecule into monomers
 *
 * Every atom should belong to either 1 or 2 monomers. If an atom belongs to 2
 * monomers, it represents a connection between the two monomers.
 *
 * Input ROMol is labeled as follows:
 * - firstMonomerIdx: index of the first monomer given atom belongs to
 * - secondMonomerIdx: index of the second monomer given atom belongs to (this
 * is optional, means there is a bond between two monomers)
 *
 * Returns a list monomer index sets, which represents the atom indices of each
 * monomer.
 */
void identify_monomers(RDKit::ROMol& atomistic_mol,
                       std::vector<std::vector<int>>& monomers,
                       std::vector<Linkage>& linkages)
{
    // Approach for identifying monomers:
    // 1. Find all matches with SMARTS queries for amino acids (TODO: Nucleic
    // acids & CHEM)
    // 2. Add side chains to generic matches based on attachment points
    // 3. Identify and group any remaining atoms into 'unclassified' monomers.
    // These are grouped
    //    by connectivity.
    std::unordered_map<unsigned int, unsigned int> attch_pts;
    add_matches_to_monomers(GENERIC_AMINO_ACID_QUERY, atomistic_mol, monomers,
                            attch_pts, linkages);
    add_matches_to_monomers(GLYCINE_AMINO_ACID_QUERY, atomistic_mol, monomers,
                            attch_pts, linkages);
    // TODO: nucleic acids and CHEM monomers

    // now, add sidechains onto each monomer
    for (size_t monomer_idx = 0; monomer_idx < monomers.size(); ++monomer_idx) {
        if (attch_pts.find(monomer_idx) != attch_pts.end()) {
            // there is a sidechain to add!
            add_sidechain_to_monomer(atomistic_mol, monomers[monomer_idx],
                                     monomer_idx, attch_pts[monomer_idx]);
        }
    }
}

void neutralize_atoms(RDKit::ROMol& mol)
{
    // Algorithm for neutralizing molecules from
    // https://www.rdkit.org/docs/Cookbook.html#neutralizing-molecules by Noel
    // O’Boyle Will neutralize the molecule by adding or removing hydrogens as
    // needed. This will ensure SMILES can be used to match atomistic structures
    // to the correct monomer.
    static const std::unique_ptr<RDKit::RWMol> neutralize_query(
        RDKit::SmartsToMol(
            "[+1!h0!$([*]~[-1,-2,-3,-4]),-1!$([*]~[+1,+2,+3,+4])]"));
    for (const auto& match : RDKit::SubstructMatch(mol, *neutralize_query)) {
        auto atom = mol.getAtomWithIdx(match[0].second);
        auto chg = atom->getFormalCharge();
        auto hcount = atom->getTotalNumHs();
        atom->setFormalCharge(0);
        atom->setNumExplicitHs(hcount - chg);
        atom->updatePropertyCache();
    }
}

void build_cg_mol(const RDKit::ROMol& atomistic_mol,
                  std::vector<std::vector<int>>& monomers,
                  boost::shared_ptr<RDKit::RWMol> cg_mol,
                  std::vector<Linkage>& linkages)
{
    // Start with all atoms in a single peptide chain
    cg_mol->setProp<bool>(HELM_MODEL, true);

    constexpr bool isomeric_smiles = false;
    int residue_num = 1;
    for (const auto& monomer : monomers) {
        auto monomer_smiles = RDKit::MolFragmentToSmiles(
            atomistic_mol, monomer, nullptr, nullptr, nullptr, isomeric_smiles);
        // We have to roundtrip to canonicalize smiles -- see RDKit issue #7214
        std::unique_ptr<RDKit::RWMol> canon_mol(
            RDKit::SmilesToMol(monomer_smiles));
        neutralize_atoms(*canon_mol);
        monomer_smiles = RDKit::MolToSmiles(*canon_mol);

        // If the monomer is a known amino acid, use the 1-letter code
        if (amino_acids.find(monomer_smiles) != amino_acids.end()) {
            add_monomer(*cg_mol, amino_acids.at(monomer_smiles), residue_num,
                        "PEPTIDE1");
        } else {
            add_monomer(*cg_mol, monomer_smiles, residue_num, "PEPTIDE1",
                        MonomerType::SMILES);
        }
        ++residue_num;

        // TODO: Check for known nucleic acids and CHEM monomers
    }

    for (const auto& link : linkages) {
        // TODO: Non forward linkages
        add_connection(*cg_mol, link.monomer_idx1, link.monomer_idx2,
                       link.to_string());
    }
}

void remove_waters(RDKit::RWMol& mol)
{
    mol.beginBatchEdit();
    auto is_water = [](const RDKit::Atom* atom) {
        const auto res_info = dynamic_cast<const RDKit::AtomPDBResidueInfo*>(
            atom->getMonomerInfo());
        if (res_info && res_info->getResidueName() == "HOH ") {
            return true;
        }
        return false;
    };
    for (auto atom : mol.atoms()) {
        if (is_water(atom)) {
            mol.removeAtom(atom);
        }
    }
    mol.commitBatchEdit();
}

void find_chains_and_residues(const RDKit::ROMol& mol,
                              ChainsAndResidues& chains_and_residues)
{
    // Find all chains and residues in the molecule
    for (unsigned int i = 0; i < mol.getNumAtoms(); ++i) {
        const auto atom = mol.getAtomWithIdx(i);
        const auto res_info = static_cast<const RDKit::AtomPDBResidueInfo*>(
            atom->getMonomerInfo());
        const auto chain_id = res_info->getChainId();
        const auto res_num = res_info->getResidueNumber();
        const auto ins_code = res_info->getInsertionCode();
        chains_and_residues[chain_id][std::make_pair(res_num, ins_code)]
            .push_back(i);
    }
}

std::string get_monomer_smiles(RDKit::ROMol& mol,
                               const std::vector<unsigned int>& atom_idxs,
                               const std::pair<int, std::string>& current_key,
                               int res_num, bool end_of_chain)
{
    // Determine the atoms in current_res that connect to adjacent residues
    std::vector<std::pair<int, int>> attch_idxs; // adjacent res num, atom_idx
    for (auto idx : atom_idxs) {
        const auto at = mol.getAtomWithIdx(idx);
        for (const auto neigh : mol.atomNeighbors(at)) {
            const auto res_info =
                dynamic_cast<const RDKit::AtomPDBResidueInfo*>(
                    neigh->getMonomerInfo());
            const auto key = std::make_pair(res_info->getResidueNumber(),
                                            res_info->getInsertionCode());
            if (key != current_key) {
                // neighbor is in different residue, this will be an attachment
                // point
                attch_idxs.push_back({res_num, at->getIdx()});
            }
        }
    }
    // Attachment point order is dependent on residue order
    std::sort(attch_idxs.begin(), attch_idxs.end(),
              [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
                  return a.first < b.first;
              });
    static constexpr bool sanitize = false;
    auto mol_fragment = ExtractMolFragment(mol, atom_idxs, sanitize);

    // Add dummy atoms with an attachment point #s
    // For now, all monomers have at least attachment points 1 and 2 so
    // that backbone bonds can be formed (except the beginning monomer)
    // TODO: non-backbone linkages and attachment points, see SHARED-10995
    int current_attchpt = (res_num == 1) ? 2 : 1;
    static constexpr bool take_ownership = true;
    static constexpr bool update_label = true;
    for (const auto& [_, ref_idx] : attch_idxs) {
        for (auto at : mol_fragment->atoms()) {
            int orig_idx;
            if (at->getPropIfPresent(REFERENCE_IDX, orig_idx) &&
                orig_idx == ref_idx) {
                auto new_at = new RDKit::Atom(0);
                new_at->setProp(RDKit::common_properties::molAtomMapNumber,
                                current_attchpt);
                auto new_at_idx =
                    mol_fragment->addAtom(new_at, update_label, take_ownership);
                mol_fragment->addBond(new_at_idx, at->getIdx(),
                                      RDKit::Bond::SINGLE);
                ++current_attchpt;
            }
        }
    }

    // There should always be enough attachment points so that
    // backbone connections can be made (R1 and R2)
    // TODO: Should this indicate a new chain?
    while (current_attchpt <= MIN_ATTCHPTS) {
        if (end_of_chain && current_attchpt > 1) {
            break;
        }
        auto new_at = new RDKit::Atom(0);
        new_at->setProp(RDKit::common_properties::molAtomMapNumber,
                        current_attchpt);
        mol_fragment->addAtom(new_at, update_label, take_ownership);
        ++current_attchpt;
    }

    // removing hydrogens to keep HELM string readable
    removeHs(*mol_fragment);
    return RDKit::MolToSmiles(*mol_fragment);
}

bool same_monomer(const std::string& smiles, const std::string& db_smiles)
{
    // Occasionally SMILES that cannot be kekulized are extracted from atomistic
    // mol, so skip sanitization
    constexpr int debug = 0;
    constexpr bool sanitize = false;

    // Monomer from original atomistic mol and monomer defined by database
    std::unique_ptr<RDKit::RWMol> mol(
        RDKit::SmilesToMol(smiles, debug, sanitize));
    std::unique_ptr<RDKit::RWMol> db_mol(
        RDKit::SmilesToMol(db_smiles, debug, sanitize));

    // Remove stereochemistry, atom map numbers, and neutralize the atoms
    // leaving groups are removed since they won't always be included in the
    // residue extracted from the atomistic molecule
    auto clean_mol = [](RDKit::RWMol& mol) {
        RDKit::MolOps::removeStereochemistry(mol);
        neutralize_atoms(mol);
        removeHs(mol);
        mol.beginBatchEdit();
        for (auto at : mol.atoms()) {
            if (at->hasProp(RDKit::common_properties::molAtomMapNumber)) {
                mol.removeAtom(at);
            }
        }
        mol.commitBatchEdit();
    };
    clean_mol(*mol);
    clean_mol(*db_mol);

    // The DB monomer has had the leaving groups removed, while the residue
    // extracted from the atomistic mol may still have them present if it is at
    // the beginning or end of a chain. As a result, we need to allow for the DB
    // monomer to have one less atom than the residue extracted from the
    // atomistic mol
    auto match = RDKit::SubstructMatch(*mol, *db_mol);
    int db_count = db_mol->getNumAtoms();
    int count = mol->getNumAtoms();
    return match.size() && (std::abs(db_count - count) <= 1);
}

boost::shared_ptr<RDKit::RWMol>
annotated_atomistic_to_cg(const RDKit::ROMol& input_mol)
{
    // Make RWMol and remove waters
    RDKit::RWMol mol(input_mol);
    remove_waters(mol);

    // Set reference index for SMILES fragments
    for (auto at : mol.atoms()) {
        at->setProp(REFERENCE_IDX, at->getIdx());
    }

    // Map chain_id -> {residue mols}
    ChainsAndResidues chains_and_residues;
    find_chains_and_residues(mol, chains_and_residues);

    // Monomer database connection to verify monomers and get HELM info
    cg_monomer_database db(get_cg_monomer_db_path());
    std::map<ChainType, unsigned int> chain_counts = {{ChainType::PEPTIDE, 0},
                                                      {ChainType::RNA, 0},
                                                      {ChainType::DNA, 0},
                                                      {ChainType::CHEM, 0}};
    boost::shared_ptr<RDKit::RWMol> cg_mol = boost::make_shared<RDKit::RWMol>();
    for (const auto& [chain_id, residues] : chains_and_residues) {
        // Use first residue to determine chain type. We assume that PDB data
        // is correct and there aren't multiple chain types in a single chain.
        // TODO: Actually check for this
        // What if the first residue is unknown?
        // note: residues are 1-indexed
        const auto first_res_info =
            dynamic_cast<const RDKit::AtomPDBResidueInfo*>(
                mol.getAtomWithIdx(residues.begin()->second[0])
                    ->getMonomerInfo());
        auto res_name = first_res_info->getResidueName();
        res_name.erase(
            std::remove_if(res_name.begin(), res_name.end(), ::isspace),
            res_name.end());

        // Default chain type is PEPTIDE if not specified.
        auto helm_info = db.get_helm_info(res_name);
        auto chain_type =
            helm_info ? std::get<2>(*helm_info) : ChainType::PEPTIDE;
        std::string helm_chain_id = fmt::format("{}{}", to_string(chain_type),
                                                ++chain_counts[chain_type]);
        int last_monomer = -1;
        // Assuming residues are ordered correctly
        size_t res_num = 1;
        for (const auto& [key, atom_idxs] : residues) {
            const auto res_info =
                dynamic_cast<const RDKit::AtomPDBResidueInfo*>(
                    mol.getAtomWithIdx(atom_idxs[0])->getMonomerInfo());
            res_name = res_info->getResidueName();
            res_name.erase(
                std::remove_if(res_name.begin(), res_name.end(), ::isspace),
                res_name.end());

            // Determine whether every residue with the number has the same PDB
            // code
            bool same_code = true;
            for (const auto& atom_idx : atom_idxs) {
                const auto atom_res_info =
                    dynamic_cast<const RDKit::AtomPDBResidueInfo*>(
                        mol.getAtomWithIdx(atom_idx)->getMonomerInfo());
                if (atom_res_info->getResidueName() != res_name) {
                    same_code = false;
                    break;
                }
            }

            helm_info = db.get_helm_info(res_name);
            bool end_of_chain = res_num == residues.size();
            size_t this_monomer;
            if (helm_info) {
                // Standard residue in monomer DB, Verify that the fragment
                // labeled as the residue matches what is in the monomer
                // database
                auto smiles = get_monomer_smiles(mol, atom_idxs, key, res_num,
                                                 res_num == residues.size());
                auto db_smiles = std::get<1>(*helm_info);

                if (same_monomer(smiles, db_smiles)) {
                    this_monomer = add_monomer(*cg_mol, std::get<0>(*helm_info),
                                               res_num, helm_chain_id);
                } else {
                    this_monomer =
                        add_monomer(*cg_mol, smiles, res_num, helm_chain_id,
                                    MonomerType::SMILES);
                }
            } else if (!same_code && backup_res_table.find(res_name) !=
                                         backup_res_table.end()) {
                // Standard residue not in monomer DB. 1-letter code is stored
                // through map
                this_monomer = add_monomer(
                    *cg_mol, std::string{backup_res_table.at(res_name)},
                    res_num, helm_chain_id);
            } else {
                auto smiles = get_monomer_smiles(mol, atom_idxs, key, res_num,
                                                 end_of_chain);
                this_monomer = add_monomer(*cg_mol, smiles, res_num,
                                           helm_chain_id, MonomerType::SMILES);
            }
            if (last_monomer != -1) {
                // Add linkage. For now we assume all linkages are backbone
                // linkages and there are no cycles
                add_connection(*cg_mol, last_monomer, this_monomer,
                               BACKBONE_LINKAGE);
            }
            last_monomer = this_monomer;
            ++res_num;
        }
    }

    return cg_mol;
}

bool has_residue_info(const RDKit::ROMol& mol)
{
    return std::any_of(mol.atoms().begin(), mol.atoms().end(),
                       [](const auto& atom) {
                           return atom->getMonomerInfo() &&
                                  atom->getMonomerInfo()->getMonomerType() ==
                                      RDKit::AtomMonomerInfo::PDBRESIDUE;
                       });
}

} // unnamed namespace

boost::shared_ptr<RDKit::RWMol> atomistic_to_cg(const RDKit::ROMol& mol,
                                                bool use_residue_info)
{
    // Use residue information to build the CG molecule. Assumes that the
    // residue information is correct, and throws if any residue information
    // is missing.
    if (use_residue_info) {
        if (!has_residue_info(mol)) {
            throw std::runtime_error(
                "No residue information found in molecule");
        }
        auto cg_mol = annotated_atomistic_to_cg(mol);
        assign_chains(*cg_mol);
        return cg_mol;
    }

    // Work on copy, for now
    RDKit::ROMol atomistic_mol(mol);
    std::vector<std::vector<int>> monomers;
    std::vector<Linkage> linkages;
    identify_monomers(atomistic_mol, monomers, linkages);

    boost::shared_ptr<RDKit::RWMol> cg_mol = boost::make_shared<RDKit::RWMol>();
    build_cg_mol(atomistic_mol, monomers, cg_mol, linkages);
    assign_chains(*cg_mol);

    // TODO
    // Now that we have the CG mol, we need to set the properties needed by the
    // HELM writer and other functions that work with CG mols created by the
    // HELM parser. I think this will include a few steps
    // 1. Break the CG Mol into polymers -- this would be done by connectivity
    // and monomer type (peptide, rna, dna, chem)
    // 2. Insure that the linkage information is correct -- backbone vs not, etc
    // 3. Set the polymers as substance groups on the molecule, and set
    // monomer-specific properties
    // 4. Maybe: make sure CG monomer indices are in connectivity order

    return cg_mol;
}

std::vector<std::vector<int>> get_monomers(const RDKit::ROMol& mol)
{
    RDKit::ROMol atomistic_mol(mol);
    std::vector<std::vector<int>> monomers;
    std::vector<Linkage> linkages;
    identify_monomers(atomistic_mol, monomers, linkages);
    return monomers;
}

} // namespace rdkit_extensions
} // namespace schrodinger
